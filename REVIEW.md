# Subject review — Codexion v1.5

Reviewed against `../en.subject.pdf`, especially Chapters V–VII.

A later audit against `../intra_test.txt`, with exact benchmark results and
updated Git/tooling observations, is available in [SHEET_REVIEW.md](SHEET_REVIEW.md).
The observed EDF startup failure has now been corrected and added to the
regression suite. **This is not a certification of full compliance**: universal
EDF liveness is not proven, and Helgrind warnings remain unresolved.

See [TESTING.md](TESTING.md) for reproducible checks and
[PROJECT_EXPLANATION.md](PROJECT_EXPLANATION.md) for the implementation walkthrough.

## Corrections applied

1. **Per-dongle arbitration (Chapter VI).** The previous grant code skipped a
   higher-priority waiter if its other dongle was unavailable. This allowed
   FIFO overtaking and EDF priority inversion. A grant now requires the coder
   at both heap roots. Granted entries are removed with `heap_pop()`. Heap
   reads now also hold the dongle mutexes.
2. **Global stopping condition (Chapters IV–VI).** Previously each coder stopped
   requesting dongles at its own quota, and the monitor excluded it. The subject
   describes continuous cycling until everyone meets the quota or anyone burns
   out; it gives no individual exemption. Coders now remain active and monitored
   until global stop. Counts saturate to avoid overflow on long runs.
3. **Completion versus burnout.** A worker could record the final completion
   before the monitor acquired the mutex, hiding an already expired deadline.
   Completion now checks all burnout deadlines before incrementing its count.
   The separate monitor remains responsible for waking at the earliest deadline.
4. **Documentation and tests.** Added an executable `make test` target and
   regression checks. Removed references to missing `test-scale` and
   `DEFENSE_LIFO.md`, unsupported correction-sheet claims, and duplicate titles.

5. **EDF startup.** Register all first EDF requests under the simulation mutex
   before releasing the barrier. Gate only each coder's first acquisition at a
   phase derived from ring size, compilation/cooldown, and phase durations.
   This prevents early workers from repeatedly overtaking neighbors whose first
   requests had not yet been registered and avoids the observed bunched-start
   failure. Later requests are immediate after refactoring; per-dongle heap
   ordering and cooldown checks remain unchanged.

## Requirement assessment

| Requirement | Assessment |
| --- | --- |
| C, allowed external functions, no application globals | Source and imported-symbol inspection passed |
| Required compiler flags and Makefile targets | Build passed; second make performs no relinking |
| 42 Norm | All production C/header files passed `norminette src` |
| Mandatory arguments and scheduler validation | Malformed-input tests passed |
| Coder threads and separate monitor | Present, with startup synchronization and joins |
| Ring topology and one-coder case | Inspected; one coder takes one dongle then burns out |
| Dongle mutexes and cooldown | Present; deterministic and log-based tests passed |
| Custom FIFO/EDF heaps and deterministic tie-break | Present; priority regression tests added |
| Global completion and burnout stopping | Corrected and tested |
| EDF liveness for feasible parameters | Observed startup failure corrected; repeated regression passes; universal guarantee unproven |
| Serialized logs and burnout within 10 ms | Passed exercised timing cases; not a universal timing guarantee |
| Allocation and cleanup | Valgrind representative run: zero errors, zero bytes remaining |
| Thread analysis | DRD: zero errors; Helgrind: two warnings remain |
| README requirements | Required sections and italic attribution present |
| Submission layout | Submit the contents of `projet/`, putting Makefile and README at repository root |

## EDF regression and its correction

Before startup staggering, this command produced `300 4 burned out`:

```sh
./codexion 5 300 20 20 20 10 80 edf
```

The revised implementation completes this workload in three repeated automated
runs, and also completes the 7-coder variant. `tests/check.py` treats burnout
in these cases as a failure, rather than accepting it as an expected outcome.

For the 5-coder case, the computed initial cycle is 254 ms. First eligibility
follows coder order 1, 4, 2, 5, 3, at phases 0, 50, 101, 152 and 203 ms. All
initial requests already exist in both neighboring queues, so later requests
cannot bypass initial waiters. After the first compilation there is no phase
time gate: ordinary FIFO/EDF heap and resource rules decide access.

This correction addresses the reproduced failure. It does not prove that every
feasible input meets its deadlines under every OS schedule. In particular,
very tight timing margins, slow stdout, and real-time clock adjustments need
care on the evaluation machine. Strict priority is still enforced; the fix
does not restore the earlier priority-bypass behavior.

## Verification details and limits

- `make test`: invalid inputs, phase transitions and durations, resource
  ownership, cooldown, completion, zero durations/quota, one-coder burnout,
  burnout during compilation, strict priority bypass prevention, and distinct
  FIFO/EDF ordering. Cases include 2, 3, 4, 5 and 20 coders, plus the repeated
  tight 5-coder EDF regression and a 7-coder regression.
- `norminette src`: all files passed after the source corrections.
- `make` repeated without edits: nothing to rebuild.
- `valgrind --leak-check=full --errors-for-leak-kinds=all --error-exitcode=99
  ./codexion 5 2000 10 10 10 3 10 edf`: 13 allocations, 13 frees, zero errors.
- `valgrind --tool=drd --error-exitcode=99
  ./codexion 5 5000 10 10 10 3 10 edf`: zero errors on the revised program.
- `valgrind --tool=helgrind --error-exitcode=99
  ./codexion 5 5000 10 10 10 3 10 edf`: reported two condition-variable
  lock-ownership warnings originating inside libc's `pthread_cond_timedwait`.
  The displayed reports did not identify application data races. Application
  broadcasts were inspected and hold the simulation mutex, but the warnings
  have not been independently resolved; do not report this as a clean race check.

The tests cannot prove all schedules safe. Thread-creation failure and allocation
failure paths were inspected but not exhaustively fault-injected. Timed waits
use the condition variable's default real-time clock while elapsed times use
monotonic time; clock adjustments and OS/output stalls remain timing limits.

The author login in the original README is `zhamza`; confirm that this is the
correct submission login. No Git commit was made: this workspace does not expose
a usable Git repository. Original source and documentation were copied to
`/tmp/codexion-before/` before editing (temporary backup).

## Applied follow-up: even-ring FIFO startup

The reproduced `10 325 100 100 100 10 0 fifo` failure is addressed by
`src/fifo_start.c`. Each worker's first request enters in odd-ID order, then
even-ID order, and all workers finish registration before acquiring resources.
Later requests retain normal FIFO order. The main regression suite now includes
this case. Odd-ring FIFO and EDF scheduling were not changed by this follow-up.
The archived design and patch are in `drafts/FIFO_STARTUP_DRAFT.md`.
