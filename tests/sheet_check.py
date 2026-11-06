#!/usr/bin/env python3
"""Run the exact intra_test.txt benchmarks; keep stdout and JSON evidence."""
import argparse
import json
import pathlib
import re
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
CASES = [
    ('easy_single', [1, 800, 200, 200, 200, 10, 0, 'fifo'], 'burnout'),
    ('easy_fifo', [5, 2000, 200, 200, 200, 10, 0, 'fifo'], 'complete'),
    ('easy_edf', [5, 2000, 200, 200, 200, 7, 0, 'edf'], 'complete'),
    ('less_easy', [5, 500, 200, 200, 200, 10, 0, 'fifo'], 'burnout'),
    ('medium_400', [5, 3000, 200, 200, 200, 10, 400, 'fifo'], 'complete'),
    ('compare_fifo_800', [5, 3000, 200, 200, 200, 10, 800, 'fifo'], 'compare'),
    ('compare_edf_800', [5, 3000, 200, 200, 200, 10, 800, 'edf'], 'compare'),
]
PATTERN = re.compile(r'(\d+) (\d+) (has taken a dongle|is compiling|is debugging|is refactoring|burned out)')


def inspect(output, args, expected):
    n, burnout, compile_ms, debug_ms, refactor_ms, required, cooldown, _ = args
    events = []
    for line in output.splitlines():
        match = PATTERN.fullmatch(line)
        assert match, f'malformed log: {line!r}'
        t, coder = map(int, match.group(1, 2))
        assert 1 <= coder <= n, line
        assert not events or t >= events[-1][0], line
        events.append((t, coder - 1, match[3]))
    assert events, 'empty output'
    last = [0] * n
    phase = [0] * n
    state = ['initial'] * n
    starts = [0] * n
    completed = [0] * n
    owners = [None] * n
    available = [0] * n
    grant_order = []
    dead = False
    delay = None
    for i, (t, c, event) in enumerate(events):
        assert not dead, 'output after burnout'
        pair = (c, (c + 1) % n)
        if event == 'has taken a dongle':
            continue
        if event == 'is compiling':
            assert n > 1 and i >= 2, 'cannot compile without two dongles'
            assert all(e[1:] == (c, 'has taken a dongle') for e in events[i-2:i]), 'missing consecutive dongle messages'
            assert i == 2 or events[i-3][1:] != (c, 'has taken a dongle'), 'more than two dongle messages'
            assert t - last[c] < burnout + 2, 'compile after deadline'
            if state[c] != 'initial':
                assert state[c] == 'refactoring', 'invalid phase order'
                assert t - phase[c] >= refactor_ms - 2, 'refactor too short'
            for d in pair:
                assert owners[d] is None, 'duplicated dongle ownership'
                assert t >= available[d] - 2, 'cooldown too short'
                owners[d] = c
            starts[c] += 1
            grant_order.append(c + 1)
            last[c] = phase[c] = t
            state[c] = 'compiling'
        elif event == 'is debugging':
            assert state[c] == 'compiling', 'debug before compile'
            assert t - last[c] >= compile_ms - 2, 'compile too short'
            completed[c] += 1
            for d in pair:
                assert owners[d] == c, 'release without ownership'
                owners[d] = None
                available[d] = t + cooldown
            state[c] = 'debugging'
            phase[c] = t
        elif event == 'is refactoring':
            assert state[c] == 'debugging', 'refactor before debug'
            assert t - phase[c] >= debug_ms - 2, 'debug too short'
            state[c] = 'refactoring'
            phase[c] = t
        elif event == 'burned out':
            delay = t - last[c] - burnout
            assert -1 <= delay <= 10, f'burnout delay {delay} ms'
            dead = True
    if n == 1:
        assert sum(e[2] == 'has taken a dongle' for e in events) == 1
        assert not any(starts)
    if expected == 'burnout':
        assert dead, 'expected burnout'
    if expected == 'complete':
        assert not dead, 'unexpected burnout'
    if not dead:
        # Final completed compile does not emit a debugging message.
        # Logs permit inference, not direct observation, of this last completion.
        inferred = [completed[c] + (state[c] == 'compiling') for c in range(n)]
        assert min(starts) >= required and min(inferred) >= required, 'quota not reached'
    return dict(outcome='burnout' if dead else 'completion', last_ms=events[-1][0],
                burnout_delay_ms=delay, compile_starts=starts,
                observed_completions=completed, grant_order=grant_order)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repeats', type=int, default=3)
    args = parser.parse_args()
    if args.repeats < 1:
        parser.error('--repeats must be positive')
    folder = pathlib.Path(tempfile.mkdtemp(prefix='codexion-sheet-'))
    print(f'Evidence: {folder}', flush=True)
    results = []
    for name, command, expected in CASES:
        for repeat in range(1, args.repeats + 1):
            row = dict(case=name, repeat=repeat, args=command, expected=expected)
            try:
                proc = subprocess.run([str(ROOT / 'codexion'), *map(str, command)],
                                      capture_output=True, text=True, timeout=60)
                (folder / f'{name}-{repeat}.log').write_text(proc.stdout)
                (folder / f'{name}-{repeat}.stderr').write_text(proc.stderr)
                assert proc.returncode == 0, f'exit {proc.returncode}: {proc.stderr}'
                row.update(inspect(proc.stdout, command, expected))
                row['status'] = 'PASS'
            except (AssertionError, subprocess.TimeoutExpired) as exc:
                row.update(status='FAIL', error=str(exc))
            results.append(row)
            (folder / 'results.json').write_text(json.dumps(results, indent=2))
            print(f"{name} #{repeat}: {row['status']} "
                  f"{row.get('outcome', row.get('error'))} "
                  f"{row.get('last_ms', '')} ms", flush=True)
    failures = sum(row['status'] == 'FAIL' for row in results)
    print(f'{len(results) - failures}/{len(results)} benchmark runs passed; {folder}')
    print('Comparison cases permit burnout; request order requires queue-level inspection.')
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
