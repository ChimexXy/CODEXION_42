*This project has been created as part of the 42 curriculum by zhamza.*

# Codexion

A C / POSIX-threads resource-arbitration simulation inspired by the dining
philosophers problem. It models coders sharing USB dongles in a circular hub,
where every compilation needs **both** adjacent dongles, every release incurs a
**cooldown**, and competing requests are ordered using **FIFO or EDF**.


See [TESTING.md](TESTING.md) for test commands and expected results, and
[PROJECT_EXPLANATION.md](PROJECT_EXPLANATION.md) for a complete code walkthrough.
See [SHEET_REVIEW.md](SHEET_REVIEW.md) for the supplied correction-sheet results.

## Description

There are `N` coders and `N` shared dongles. Each coder loops through:

1. Acquire their two neighboring dongles **atomically**.
2. Compile while both dongles are held.
3. Release both, initiating each dongle's cooldown.
4. Debug and refactor without holding dongles, then request them again.

Every coder has a deadline: `last_compile_start + time_to_burnout`. At the
start of the simulation, `last_compile_start` is the common simulation start.
A monitor thread stops the simulation at the earliest missed deadline. It also
stops once every coder has **completed** the required number of compilations.

## Instructions

Requires a C compiler with POSIX threads, `make`, and (for the optional test
suite) Python 3. No external C libraries or libft are required.

```sh
make
./codexion 5 800 200 200 100 3 50 edf
./codexion 5 800 200 200 100 3 50 fifo
make test
make clean
make fclean
make re
```

All eight positional arguments are mandatory:

| Position | Name | Meaning |
| --- | --- | --- |
| 1 | `number_of_coders` | Positive integer; also number of dongles |
| 2 | `time_to_burnout` | Milliseconds from start of last compilation to next start deadline |
| 3 | `time_to_compile` | Milliseconds holding both dongles |
| 4 | `time_to_debug` | Milliseconds debugging |
| 5 | `time_to_refactor` | Milliseconds refactoring |
| 6 | `number_of_compiles_required` | Stop after all have completed at least this many |
| 7 | `dongle_cooldown` | Minimum milliseconds from release to reacquisition |
| 8 | `scheduler` | Exactly `fifo` or `edf` |

Invalid numbers, negatives, overflow, wrong argument count, invalid scheduler,
and zero coders are rejected. Zero durations and zero required compilations are
accepted. With zero required compilations, the program exits immediately.

### Logs

The program emits only these event forms to stdout:

```text
0 1 has taken a dongle
0 1 has taken a dongle
0 1 is compiling
200 1 is debugging
400 1 is refactoring
```

A single coder has only one dongle; it takes that dongle once and cannot start
compiling. A separate monitor eventually prints `<timestamp> 1 burned out`.

## Architecture

| File | Responsibility |
| --- | --- |
| `src/main.c` | CLI and lifecycle |
| `src/parsing.c` | Strict nonnegative, overflow-safe argument parsing |
| `src/init.c`, `src/cleanup.c` | Structures, mutexes, queues, allocation and cleanup |
| `src/threads.c` | Worker/monitor startup barrier, joins and creation failures |
| `src/worker.c`, `src/worker_phases.c` | Compilation, debugging, refactoring |
| `src/acquire.c` | Request creation and waiting |
| `src/fifo_start.c` | Ordered first-request admission for even FIFO rings |
| `src/grant.c`, `src/grant_wait.c` | Pair arbitration and cooldown wake-ups |
| `src/dongle.c` | Per-dongle mutex discipline, pair locking and release |
| `src/heap.c`, `src/heap_pop.c` | Custom binary min-heap priority queues |
| `src/monitor.c` | Earliest-deadline burnout detection for every coder |
| `src/edf_schedule.c` | Initial EDF staggering and deterministic priority tie-break |
| `src/timing.c` | Monotonic elapsed time; condition-variable timed waits |
| `src/logging.c` | Serialized, direct `write(1, ...)` output to stdout |

### Dongle mapping

Coder `i` (zero-based) needs dongles `i` and `(i + 1) % N`, ensuring that
neighboring coders share exactly one dongle. For `N == 2`, both coders share
both dongles. For `N == 1`, there is only one dongle and no compilation.

### FIFO / EDF priority heaps and atomic reservations

Every dongle has a binary min-heap of requests from its neighboring coders.
A request is enqueued on both dongles while holding the simulation mutex.
All initial EDF requests are registered before the startup barrier opens.
For even FIFO rings, workers register their first requests in odd-ID order,
then even-ID order, and wait until every first request is queued before
acquiring dongles. Later FIFO requests and odd-ring FIFO startup use ordinary
worker request arrival order. It can take both dongles only when both are free, both cooldowns have
expired, and its request is at the root of **both** heaps. A higher-priority
waiter is never bypassed just because its other dongle is unavailable.
Granted requests are removed using `heap_pop()`, which preserves heap order.
All heap and ownership accesses hold the corresponding dongle mutexes.

- **FIFO**: increasing request sequence, then coder ID.
- **EDF**: earliest absolute `request_deadline` first. Equal deadlines use
  `edf_slot()`, then request sequence and coder ID. First acquisitions also
  wait until their initial `edf_slot()` time, spreading demand around the ring.
  After the first compilation, this value serves only as a tie-break: requests
  are submitted immediately after refactoring without a recurring time gate.

`init_schedule()` bases startup spacing on compilation plus cooldown and on
one coder's full phase cycle. It adds a small margin only when the calculated
cycle remains below the burnout interval. The repeated 5-coder, 300-ms burnout,
80-ms cooldown regression now completes. This improves observed liveness
without allowing lower-priority requests to bypass a heap root.

Each heap holds at most two requests, one per adjacent coder. Request keys
remain unchanged while queued. Strict priority on both heaps prevents cyclic
queue dependencies: the globally highest-priority pending request cannot be
blocked by a lower-priority request in a queue. Occupied resources and cooldowns
can still delay it. This does not prove that all feasible real-time workloads
meet their deadlines; see `REVIEW.md` for validation limits.

## Blocking cases handled

### Deadlock and Coffman's conditions

The classic four Coffman conditions are mutual exclusion, hold-and-wait,
no preemption, and circular wait. Mutual exclusion is necessary for these
resources, but **hold-and-wait is prevented**: both dongles are reserved
atomically under the simulation mutex, or neither is reserved. There is no
partial dongle ownership while waiting. Per-dongle heap priorities are
strictly ordered, preventing queue-order cycles; mutex acquisitions are
ordered by dongle index.

### Starvation prevention

FIFO ranks requests in arrival order. EDF ranks requests by earliest burnout
deadline with a deterministic tie-break. Later requests cannot take a dongle
ahead of a queued higher-priority request. Atomic granting prevents partial
ownership while waiting. Coders continue their compile/debug/refactor cycles
until **all** coders have completed at least the required number of compilations,
or any coder burns out. Reaching the individual count does not exempt a coder
from burnout monitoring. Counters saturate at the required count.

### Cooldown handling

Each dongle stores `ready_at`, assigned at release as
`monotonic_now + cooldown`. Free ownership is not sufficient to grant it;
the current monotonic time must also have reached `ready_at`.
A waiting coder sleeps on a condition variable with a timed wait until the
latest of its two candidate dongle cooldowns expires, also accounting for its
initial EDF phase gate when applicable. Ordinary releases,
new requests, compilation starts, and simulation stop all notify waiters.

### Precise burnout detection

An independent monitor thread scans the coders' absolute deadlines while
holding the simulation mutex. It performs a `pthread_cond_timedwait` for the
nearest deadline, waking early when any compilation changes a deadline. When
a deadline is missed it sets the shared stop flag, logs the burnout event,
and broadcasts to all sleepers. The program uses `CLOCK_MONOTONIC` for elapsed
time and converts relative waits into `CLOCK_REALTIME` deadlines because
normal `pthread_cond_t` objects use the real-time condition clock by default.
A worker also checks overdue deadlines before counting a completed compile,
so completion cannot mask an already missed deadline. No busy-waiting is needed. Logged timestamps have millisecond resolution;
OS scheduling may introduce small delays.

### Log serialization

A dedicated logging mutex guards each complete line. Messages are assembled
in a small stack buffer and written with `write(1, ...)`, avoiding stdio
buffering delays. Event logging is called while the simulation mutex is held,
so the stop state cannot change mid-message. No messages are printed after
an observed burnout.

## Thread synchronization mechanisms

**Locking order:** `sim.mutex` is always acquired before any dongle mutex;
for two distinct dongles, the lower index is locked first. A print mutex
may be taken while holding `sim.mutex`, never the other way round. This
fixed order avoids lock-inversion deadlocks.

- `sim.mutex`: protects the shared stop/start flags, coder compilation
  counters, deadlines, request sequence and arbitration transactions.
- `dongle[i].mutex`: protects that dongle's owner, ready-at timestamp, and
  request heap. All access to these fields happens with this mutex held.
- `print_mutex`: prevents messages being interleaved during output.
- `changed` condition: prevents CPU-intensive polling; it coordinates
  request waiters, the monitor, sleeps, the start barrier, and shutdown.

For example, two adjacent coders can simultaneously attempt to request the
same dongle. Their requests are serialized by `sim.mutex`, recorded in
per-dongle heaps, and granted only by a transaction holding the dongles'
mutexes, so the physical resource cannot be duplicated or stolen. The
monitor accesses `last_start` under the same simulation mutex, so it cannot
observe a torn update while a worker starts a compilation.

## Testing

Run `make test` for malformed-input checks, both scheduling policies, phase
order and durations, completion, cooldowns, single-coder behavior, burnout
logging within 10 ms, deterministic priority-bypass regressions, and repeated EDF startup/cooldown
regressions. The tests
require Python 3 and compile a temporary C arbitration harness outside the
source directory. They use generous deadlines for successful workloads.

Additional checks:

```sh
norminette src
valgrind --leak-check=full ./codexion 5 2000 10 10 10 3 10 edf
valgrind --tool=helgrind ./codexion 4 5000 10 10 10 3 10 fifo
```

Tests are evidence for the exercised cases, not a proof of liveness or of the
10-ms bound on every machine. Slow output consumers and OS scheduling can delay
threads. See `REVIEW.md` for the subject review and remaining limitations.

## Resources

- Project subject: **Codexion v1.5**, Chapters IV–VIII.
- POSIX threads: `pthread_create(3)`, `pthread_mutex_lock(3)`,
  `pthread_cond_wait(3)`, `pthread_cond_timedwait(3)` manual pages.
- Time functions: `clock_gettime(2)` and `clock_gettime(3)` manual pages.
- Operating Systems: Three Easy Pieces, chapters on locks and condition
  variables (Remzi H. Arpaci-Dusseau and Andrea C. Arpaci-Dusseau).
- Cormen et al., *Introduction to Algorithms*, binary heaps and priority
  queues.
- Coffman et al. (1971), conditions for resource deadlocks.

### AI usage disclosure

AI assistance was used to review the subject, propose the concurrency
architecture, draft the C implementation, design a custom heap and pair
arbitration mechanism, assemble the Makefile, write documentation, and
generate initial automated tests. A subsequent AI-assisted subject review
corrected per-dongle arbitration, global completion and burnout handling,
implemented initial EDF staggering and ordered even-ring FIFO startup,
added executable regression checks, and updated this README. The generated code and explanations must
be independently reviewed, tested and understood by the submitting student,
as required by the subject. AI output is not evidence of correctness.

## Submission / defense notes

The `projet/` directory is the project root. Submit its **contents** so that
`Makefile` and `README.md` are at the root of your assigned repository.

Commit source code, headers, `Makefile`, and `README.md` to your assigned
Git repository. Do not submit only a binary. Verify Norm, run on the campus
machines, and be ready to explain the locking order, both-dongle grant,
EDF tie-break, heap operations, cooldown wake-up, monitor clock, and each
termination path. Recode tasks may be requested during peer evaluation.
