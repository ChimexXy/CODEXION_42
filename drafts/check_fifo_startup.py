#!/usr/bin/env python3
"""Check the 10-coder FIFO regression against a selected build."""
import argparse
import importlib.util
import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=pathlib.Path, default=ROOT / 'codexion')
    parser.add_argument('--repeats', type=int, default=12)
    options = parser.parse_args()
    if options.repeats < 1:
        parser.error('--repeats must be positive')
    spec = importlib.util.spec_from_file_location('sheet_check', ROOT / 'tests/sheet_check.py')
    checks = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(checks)
    args = [10, 325, 100, 100, 100, 10, 0, 'fifo']
    for run in range(1, options.repeats + 1):
        result = subprocess.run([str(options.binary.resolve()), *map(str, args)],
                                capture_output=True, text=True, timeout=8)
        assert result.returncode == 0, result.stderr
        checks.inspect(result.stdout, args, 'complete')
        starts = [[] for _ in range(10)]
        for line in result.stdout.splitlines():
            time, coder, event = line.split(' ', 2)
            if event == 'is compiling':
                starts[int(coder) - 1].append(int(time))
        largest = max(b - a for times in starts for a, b in zip(times, times[1:]))
        assert largest < 325, f'deadline exceeded: {largest} ms'
        print(f'Run {run}: PASS; maximum consecutive-start gap {largest} ms', flush=True)


if __name__ == '__main__':
    main()
