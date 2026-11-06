# Review against the supplied correction sheet

Source: `../intra_test.txt`. This assessment covers the current working tree in
`projet/`, not only the earlier subject-based review or the committed revision.

**Verdict:** the current program matches the observed functional benchmarks in
this sheet. All seven exact benchmark commands were run three times, and all
21 runs met their required outcome or comparison conditions. The remaining
qualification is thread-tool validation: FIFO still produces Helgrind warnings.
This is not an unconditional certification of a full evaluation pass.

## Section-by-section assessment

| Sheet section | Result | Evidence / qualification |
| --- | --- | --- |
| Basics | Pass in this environment | Builds using `cc -Wall -Wextra -Werror -pthread`; binary is `codexion`; parser requires eight arguments; coder threads and monitor are present |
| Global state | Pass by inspection | Shared state lives in `t_sim` and its allocated arrays; no application mutable global variables |
| Norm | Pass | Every production C/header file passes Norminette |
| Allowed functions | Pass by inspection | Application imports are within the subject list; libc startup symbols are compiler/runtime support |
| README | Matches required sections | Italic first line, Description, Instructions, Resources/AI disclosure, Blocking cases handled, Thread synchronization mechanisms |
| Easy | Pass, three runs each | Single coder burns out; both required multi-coder runs complete |
| Less easy | Pass, three runs | Required burnout at 500 ms, final line; normal runs have consecutive acquisition pairs and valid phase ordering |
| Medium: 400-ms cooldown | Pass, three runs | Completes without burnout; inferred ownership/cooldown checks pass |
| Medium: FIFO versus EDF | Consistent with sheet | FIFO and EDF differ under 800-ms cooldown; controlled queue harness confirms their priority decisions |
| Memory | Pass on checked paths | Memcheck reports zero errors and no leaks for single-coder, normal completion and burnout |
| Data races / locking | Qualified | DRD clean for FIFO and EDF; Helgrind clean for this EDF run but reports condition-variable warnings for FIFO; details below |
| FIFO-to-LIFO recode | Demonstrated in a temporary copy | Comparator test and runtime checks pass; student must still explain and perform the live exercise |
| Official repository / student explanation | Not certified by this audit | Git root is `projet/`; ownership, official submission and the student's understanding require evaluation-time verification |

## Exact benchmark results

Arguments below follow the sheet's order. Each command is run as
`./codexion <arguments>` from the project root.

| Arguments | Sheet expectation | Observed in three runs |
| --- | --- | --- |
| `1 800 200 200 200 10 0 fifo` | Must burn out near 800 ms | Burnout at 800, 801, 800 ms; no compilation |
| `5 2000 200 200 200 10 0 fifo` | Complete, no burnout | Completed all three; last log at 6004, 6205, 6205 ms |
| `5 2000 200 200 200 7 0 edf` | Complete, no burnout | Completed all three; last log at 4244 ms in each |
| `5 500 200 200 200 10 0 fifo` | Must burn out near 500 ms | Burnout at 500 ms in each; burnout was the last line |
| `5 3000 200 200 200 10 400 fifo` | Complete, respect 400-ms cooldown | Completed all three; last log at 15008, 14606, 14609 ms |
| `5 3000 200 200 200 10 800 fifo` | Compare arbitration with EDF | Burnout at 3000 ms in each; no observed ownership/cooldown violation |
| `5 3000 200 200 200 10 800 edf` | Compare arbitration with FIFO | Completed all three; last log at 24509, 24510, 24512 ms |

“Last log” is not an exact completion timestamp: the final completion does not
emit a separate success/debugging message. Completion was assessed using process
exit, compilation counts inferred from logs, and inspection of the stopping code.

### Why the 800-ms FIFO burnout is not automatically a failed sheet benchmark

The sheet explicitly requires no burnout for the Easy success cases and for
the 400-ms cooldown run. Its 800-ms paragraph instead asks for comparison of
request arbitration; it does not explicitly require successful completion under
both policies. FIFO burnout by itself is therefore not enough to fail that
comparison. It must still respect ordering, ownership, cooldown and burnout logs.

Examples of initial grant sequences from the recorded runs:

```text
FIFO, cooldown 800: 2, 1, 3, 5, 2, 1, then burnout
EDF,  cooldown 800: 1, 4, 2, 5, 3, 1, 4, 2, 5, 3, ... then completion
```

Different compilation orders alone do not prove FIFO/EDF priority: startup
staggering also changes order, and standard output does not record queue arrivals.
Source inspection confirms the comparator and both-root grant rule. A controlled
C harness additionally queued an older request with a later deadline and a newer
request with an earlier deadline: production FIFO granted the older requester,
whereas EDF granted the earlier deadline. The losing requester could not bypass
the root, and a released dongle could not be immediately reused during cooldown.

## Logging and timing checks

The benchmark runner checks complete log syntax, coder IDs, nondecreasing
timestamps, exactly two immediately preceding dongle messages for each compile,
phase ordering, inferred resource ownership and minimum durations. Ownership is
inferred from the ring mapping: the logs identify the coder, not the dongle.
Release is inferred from entry to debugging. A 2-ms allowance accounts for
millisecond timestamps and release/log separation; it does not establish an
exact bound on every internal release instant.

All observed benchmark burnout delays were between 0 and 1 ms, within the
sheet's 10-ms limit. No log appeared after burnout. Compilation/debugging waits
must last the requested time; the interval between refactoring and the next
compilation can be longer than `time_to_refactor` because acquiring resources
also takes time. `wait_phase()` implements the refactoring duration separately.

## Memory and concurrency tools

Run from `projet/`:

```sh
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=99 ./codexion 1 800 200 200 200 10 0 fifo
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=99 ./codexion 5 2000 200 200 200 10 0 fifo
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=99 ./codexion 5 500 200 200 200 10 0 fifo
valgrind --tool=drd --error-exitcode=99 ./codexion 5 2000 200 200 200 10 0 fifo
valgrind --tool=drd --error-exitcode=99 ./codexion 5 2000 200 200 200 7 0 edf
valgrind --tool=helgrind --error-exitcode=99 ./codexion 5 2000 200 200 200 10 0 fifo
valgrind --tool=helgrind --error-exitcode=99 ./codexion 5 2000 200 200 200 7 0 edf
```

Results with Valgrind 3.18.1:

| Check | Result |
| --- | --- |
| Memcheck, single coder | Zero errors; all allocations freed |
| Memcheck, successful completion | Zero errors; all allocations freed |
| Memcheck, burnout | Zero errors; all allocations freed |
| DRD, FIFO | Zero errors |
| DRD, EDF | Zero errors |
| Helgrind, FIFO | 14 reports from 8 contexts, exit 99 |
| Helgrind, EDF | Zero errors in this run |

The FIFO Helgrind contexts all concern condition-variable signal/broadcast
lock ownership inside libc's `__pthread_cond_wait_common`, called through
`pthread_cond_timedwait`. The report contains no conflicting-access data-race
trace or lock-order-cycle report. Application broadcasts were inspected and
hold the simulation mutex. Those observations distinguish these warnings from
the specific data-race/lock-order failures mentioned by the sheet, but do not
prove the warnings harmless. They remain a qualification to discuss and
investigate on the evaluation machine, not a clean Helgrind pass. No suppression
was added and production synchronization was not changed to hide the reports.

## FIFO-to-LIFO exercise

See [DEFENSE_LIFO.md](DEFENSE_LIFO.md) for the exact comparator edit and commands.
It was tested in `/tmp/codexion-sheet-lifo`; production FIFO remains FIFO.

The temporary comparator chooses the larger request sequence only in the FIFO
branch. A controlled queue test confirms the newer requester wins, while EDF
continues to choose the earlier deadline. The temporary runtime comparison used
exactly `5 3000 200 200 200 10 800 fifo`; it burned out at 3000 ms with initial
grant order `2, 5, 5, 2, 5, 2`. Its logs remained serialized and ownership/cooldown
checks passed. The sheet explicitly allows starvation in this recode.

This demonstrates that the change belongs in the queue comparator, not the
logger. It does not replace the requirement that the student perform and explain
the change during defense.

## Reproduce this review

```sh
make
norminette src
python3 tests/sheet_check.py
```

The new runner uses three repetitions of each exact sheet benchmark. It respects
the sheet's benchmark sizes and durations, including the explicit zero cooldown
cases. The separate quick developer suite uses shorter timings and is not a
substitute for this sheet-specific run.

The recorded evidence for this review is temporarily stored at:

```text
/tmp/codexion-sheet-szktoxus/results.json
/tmp/codexion-sheet-szktoxus/tools.json
/tmp/codexion-sheet-szktoxus/*.log
/tmp/codexion-sheet-szktoxus/*.txt
```

The temporary directory may be removed by the system; rerunning the script
creates a fresh directory and prints its path. Repeated passing examples do not
prove universal EDF liveness or cover every failure-injection path.

## Submission status

Git now resolves the project root to `projet/`, and the tracked `Makefile` and
`README.md` are correctly at that root. The README currently credits `zhamza`;
the evaluator must verify the actual student login and repository ownership.
The earlier review's statement that no usable Git repository was exposed was
an observation from that earlier environment, not the current state.

Current source corrections are uncommitted and the new documentation/tests are
untracked. The sheet grades the official repository: these working-tree results
do not establish that the submitted revision contains the fixes. No commit or
push was made during this review. Generated binaries and object files are also
untracked and are not a substitute for submitting source.
