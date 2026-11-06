# Codexion test guide

Run commands below from `projet/` (or from the submission root if you copied
its contents there). These tests cover the current implementation, not just
the general project idea. Read [PROJECT_EXPLANATION.md](PROJECT_EXPLANATION.md)
for the reasoning behind the code.

## 1. Build and coding style

```sh
make
make
norminette src
```

Expected: `codexion` builds with `cc -Wall -Wextra -Werror -pthread`. The second
`make` does not compile or link again. All C and header files pass Norminette.

Check the other required targets separately:

```sh
make clean
make
make fclean
make
make re
```

`clean` removes object files; `fclean` also removes the executable; `re` rebuilds.
The source files and documentation must survive all of these commands.

## 2. Automated regression suite

```sh
make test
```

Requires Python 3 and the C compiler. A successful run ends with:

```text
PASS: invalid inputs, phases, completion, cooldown, burnout timing, FIFO/EDF arbitration
```

The suite in `tests/check.py` runs each simulation with a timeout and checks:

| Area | Checks |
| --- | --- |
| Parsing | Missing/extra arguments, empty strings, negatives, decimals, non-digits, overflow, zero coders, invalid scheduler |
| Normal workloads | Both policies with 2, 3, 4, 5 and 20 coders |
| Logs | Allowed messages, valid IDs, nondecreasing timestamps, no messages after burnout |
| Compilation | Two dongle messages, no neighboring ownership conflict, phase order and durations |
| Cooldown | Reacquisition timing inferred from release/debug logs, plus direct grant checks |
| Termination | Required count, single coder, burnout during compilation, zero required count |
| Priority | No bypass of an older/earlier-deadline waiter even if its other dongle is occupied |
| FIFO versus EDF | With different arrival/deadline ordering, the correct request wins |
| Monitoring | Reaching an individual quota does not exempt a coder from burnout |
| FIFO startup regression | `10 325 100 100 100 10 0 fifo` must complete without burnout |
| EDF startup regression | Repeated 5-coder workload with 80-ms cooldown; a 7-coder variant |

The priority tests compile a small temporary C harness linking the project
sources except `main.c`. It constructs controlled queues, avoiding reliance on
random thread startup order. Its files are removed after the test.

Limits: stdout does not contain request-arrival or explicit release/completion
messages. Integration tests infer releases from debugging logs and allow 1 ms
for that log delay. They infer the final completion because shutdown suppresses
the last debugging message. These checks do not independently prove every
completion time or every possible interleaving. The direct harness covers
priority cases that output logs alone cannot establish.

## 3. Normal runs

```sh
./codexion 4 2000 10 10 10 5 10 fifo
./codexion 5 2000 10 10 10 5 10 edf
```

Expected: every coder completes at least five compilations, then the program
exits without `burned out`. Exact line ordering varies between runs. A coder
may compile more than the requested count while others are still catching up.
There is no separate success message, and the last event need not be debugging.

Do not judge success from exit code alone: a simulated burnout also returns 0.
Malformed input and initialization/thread-creation errors return nonzero.

## 4. Single coder

```sh
./codexion 1 100 20 20 20 1 10 fifo
./codexion 1 100 20 20 20 1 10 edf
```

Expected: one `has taken a dongle` line, no compilation, then one burnout near
100 ms. There is only one physical dongle, so two-dongle compilation is impossible.
The burnout must be the final line, with delay no greater than the subject's
10-ms allowance under ordinary machine load.

## 5. Burnout during compilation

```sh
./codexion 2 50 200 10 10 1 0 edf
```

Expected: burnout near 50 ms, followed by prompt shutdown. Even a compiling coder
can burn out: the deadline is measured from compilation **start**, not finish.
The monitor must not wait for the 200-ms compilation to finish.

## 6. Zero values

```sh
./codexion 4 1000 0 0 0 5 0 fifo
./codexion 4 1000 0 0 0 5 0 edf
./codexion 1 0 0 0 0 0 0 edf
./codexion 2 0 10 10 10 1 0 fifo
```

The first two should complete without burnout. Zero required compilations must
produce no simulation logs. A zero burnout duration with a positive quota causes
immediate burnout; it must not hang. This parser accepts zero timing values and
zero quota but rejects zero coders.

## 7. Invalid arguments

Run each separately:

```sh
./codexion
./codexion 0 1000 10 10 10 3 10 edf
./codexion 4 -1 10 10 10 3 10 edf
./codexion 4 1000 1.5 10 10 3 10 edf
./codexion 4 1000 abc 10 10 3 10 edf
./codexion 4 1000 '' 10 10 3 10 edf
./codexion 4 2147483648 10 10 10 3 10 edf
./codexion 4 1000 10 10 10 3 10 EDF
./codexion 4 1000 10 10 10 3 10 lifo
./codexion 4 1000 10 10 10 3 10 edf extra
```

Expected for each: nonzero exit status, `Error: invalid arguments` on stderr,
and no simulation logs on stdout. The automated suite checks rejection and
empty stdout; use these examples to inspect the human-readable error as well.

## 8. EDF liveness regression

```sh
./codexion 5 300 20 20 20 10 80 edf
./codexion 7 300 20 20 20 10 80 edf
```

Expected: completion without burnout. The 5-coder case previously burned out at
300 ms. All initial EDF requests are now queued before workers start, and their
first acquisitions are staggered to avoid that startup pattern. Later requests
are submitted immediately after refactoring; strict heap priority remains active.
The automated suite repeats the 5-coder case three times.

For a longer observation:

```sh
./codexion 5 300 20 20 20 30 80 edf > /tmp/codexion-edf.log
rg 'burned out' /tmp/codexion-edf.log
```

Expected: no match (`rg` returns 1 when no match exists). This is an additional
stress check, not a proof that EDF handles every mathematically feasible input.

## 9. Memory and thread analysis

```sh
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=99 ./codexion 5 2000 10 10 10 3 10 edf
valgrind --tool=drd --error-exitcode=99 ./codexion 5 5000 10 10 10 3 10 edf
valgrind --tool=helgrind --error-exitcode=99 ./codexion 5 5000 10 10 10 3 10 edf
```

Memory-check target: zero errors and no remaining allocated blocks. Thread-check
target: no reported races or locking errors. Use generous burnout times under
instrumentation; timing overhead makes these poor tests of the 10-ms limit.

Earlier Helgrind runs reported condition-variable lock warnings from inside
libc's `pthread_cond_timedwait`. Application broadcasts hold the simulation
mutex, and a separate DRD run found zero errors, but this does not prove those
Helgrind warnings harmless. Preserve and inspect the full report if they recur;
do not suppress them simply to obtain a green result. See [REVIEW.md](REVIEW.md)
for the recorded validation status.

## 10. Evaluation checks

- Explain why a queue winner must also wait for ownership and cooldown.
- Explain why two-dongle acquisition prevents partial-ownership deadlock.
- Verify request priorities in controlled tests, not just visible compile order.
- Run timing tests without Valgrind and with a responsive stdout consumer.
- Recheck on the evaluation machine; thread scheduling can differ.
- Put `Makefile` and `README.md` at the submission repository root.
- Understand the implementation and the remaining limits; passing these tests
  is not a guarantee of a perfect evaluation result.

## 11. Exact correction-sheet benchmarks

The original quick suite includes intentionally small timing values for developer
regressions. The supplied correction sheet instead specifies its own benchmarks
and generally excludes timings below 60 ms (while explicitly using zero cooldown).
Run its exact cases separately:

```sh
python3 tests/sheet_check.py
```

Each case runs three times, sequentially, with a 60-second timeout per run.
Allow several minutes. Use `--repeats 1` for a shorter smoke check. Raw logs and
`results.json` are saved in the temporary directory printed at startup.
The 800-ms cooldown cases are comparison cases: the sheet does not explicitly
require both to complete. The runner reports whether each completed or burned
out without treating burnout alone as a comparison failure. Queue priority is
assessed separately through source inspection and controlled heap tests.

See [SHEET_REVIEW.md](SHEET_REVIEW.md) for the recorded benchmark results and
[DEFENSE_LIFO.md](DEFENSE_LIFO.md) for the temporary recode exercise.

## 12. Repeated even-ring FIFO startup regression

The ordered first-request fix is now applied to the main source. `make test`
includes the reported ten-coder case once. To repeat it against the main binary:

```sh
make
python3 drafts/check_fifo_startup.py --repeats 10
```

Expected: all runs complete without burnout, with every coder meeting its quota
and consecutive compilation-start gaps below 325 ms. The admission order is
applied only to the first request of each coder on even FIFO rings.
