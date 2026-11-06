#!/usr/bin/env python3
"""Behavior checks and deterministic arbitration regressions (Python 3)."""
import pathlib
import re
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
BIN = ROOT / 'codexion'


def run(args):
    return subprocess.run([str(BIN), *map(str, args)], capture_output=True,
                          text=True, timeout=15)


def validate(args, burnout=False):
    result = run(args)
    assert result.returncode == 0, result.stderr
    n, limit, compile_ms, debug, refactor, required, cooldown, _ = args
    counts = [0] * n
    last = [0] * n
    state = ['initial'] * n
    phase = [0] * n
    taken = [0] * n
    ready = [0] * n
    owners = [None] * n
    dead = False
    previous = 0
    for line in result.stdout.splitlines():
        match = re.fullmatch(r'(\d+) (\d+) (has taken a dongle|is compiling|is debugging|is refactoring|burned out)', line)
        assert match, line
        time, coder = map(int, match.group(1, 2))
        event = match[3]
        coder -= 1
        assert 0 <= coder < n and time >= previous and not dead, line
        previous = time
        pair = (coder, (coder + 1) % n)
        if event == 'burned out':
            assert limit <= time - last[coder] <= limit + 10, line
            dead = True
        elif event == 'has taken a dongle':
            taken[coder] += 1
        elif event == 'is compiling':
            assert n > 1 and taken[coder] == 2, line
            assert time - last[coder] < limit, line
            if state[coder] != 'initial':
                assert state[coder] == 'refactoring', line
                assert time - phase[coder] >= refactor, line
            for dongle in pair:
                assert owners[dongle] is None and time >= ready[dongle], line
                owners[dongle] = coder
            taken[coder] = 0
            last[coder] = phase[coder] = time
            state[coder] = 'compiling'
        elif event == 'is debugging':
            assert state[coder] == 'compiling' and time - last[coder] >= compile_ms, line
            counts[coder] += 1
            state[coder] = 'debugging'
            phase[coder] = time
            for dongle in pair:
                assert owners[dongle] == coder, line
                owners[dongle] = None
                # Release precedes its log; tolerate one ms of log overhead.
                ready[dongle] = time + cooldown - 1
        else:
            assert state[coder] == 'debugging' and time - phase[coder] >= debug, line
            state[coder] = 'refactoring'
            phase[coder] = time
    assert dead == burnout, result.stdout
    if not dead:
        # The final compile completion stops the simulation before its debug log.
        for coder in range(n):
            if state[coder] == 'compiling':
                counts[coder] += 1
        assert min(counts) >= required, counts


def arbitration():
    source = r'''
#include "codexion.h"
#include <assert.h>
int main(void)
{
    t_config cfg = {4, 10000, 10, 10, 10, 2, 10, FIFO};
    t_sim sim;
    int policy;
    for (policy = FIFO; policy <= EDF; policy++) {
        cfg.policy = policy;
        assert(init_sim(&sim, &cfg));
        sim.start = now_ms();
        init_schedule(&sim);
        pthread_mutex_lock(&sim.mutex);
        sim.coders[0].compiles = 1;
        sim.coders[1].compiles = 1;
        sim.coders[0].last_start = sim.start;
        sim.coders[1].last_start = sim.start;
        sim.coders[0].request_seq = 1;
        sim.coders[1].request_seq = 2;
        sim.coders[0].request_deadline = sim.start + 9000;
        sim.coders[1].request_deadline = sim.start + 10000;
        heap_push(&sim, &sim.dongles[0].queue, 0);
        heap_push(&sim, &sim.dongles[1].queue, 0);
        heap_push(&sim, &sim.dongles[1].queue, 1);
        heap_push(&sim, &sim.dongles[2].queue, 1);
        sim.dongles[0].owner = 4;
        /* Older/earlier-deadline waiter must not be bypassed. */
        assert(!try_grant_locked(&sim.coders[1]));
        sim.dongles[0].owner = -1;
        assert(try_grant_locked(&sim.coders[0]));
        assert(heap_top(&sim.dongles[1].queue) == 1);
        release_dongles_locked(&sim.coders[0]);
        assert(!try_grant_locked(&sim.coders[1]));
        sim.dongles[0].ready_at = 0;
        sim.dongles[1].ready_at = 0;
        assert(try_grant_locked(&sim.coders[1]));
        /* A completed individual coder is still monitored until global stop. */
        sim.coders[0].compiles = cfg.required;
        sim.coders[0].last_start = now_ms() - cfg.burnout;
        sim.coders[2].last_start = sim.start;
        sim.coders[3].last_start = sim.start;
        assert(check_burnout_locked(&sim));
        pthread_mutex_unlock(&sim.mutex);
        cleanup_sim(&sim);
        assert(init_sim(&sim, &cfg));
        sim.coders[0].request_seq = 1;
        sim.coders[1].request_seq = 2;
        sim.coders[0].request_deadline = 200;
        sim.coders[1].request_deadline = 100;
        heap_push(&sim, &sim.dongles[1].queue, 0);
        heap_push(&sim, &sim.dongles[1].queue, 1);
        assert(heap_pop(&sim, &sim.dongles[1].queue)
            == (policy == EDF ? 1 : 0));
        assert(heap_pop(&sim, &sim.dongles[1].queue)
            == (policy == EDF ? 0 : 1));
        assert(heap_pop(&sim, &sim.dongles[1].queue) == -1);
        cleanup_sim(&sim);
    }
    return 0;
}
'''
    with tempfile.TemporaryDirectory() as tmp:
        path = pathlib.Path(tmp)
        (path / 'check.c').write_text(source)
        sources = [str(p) for p in (ROOT / 'src').glob('*.c') if p.name != 'main.c']
        subprocess.run(['cc', '-Wall', '-Wextra', '-Werror', '-pthread',
                        '-I', str(ROOT / 'src'), str(path / 'check.c'),
                        *sources, '-o', str(path / 'check')], check=True)
        subprocess.run([str(path / 'check')], check=True, stdout=subprocess.DEVNULL)


valid = [4, 1000, 10, 10, 10, 3, 10, 'fifo']
invalid = [[], valid[:-1], valid + ['extra']]
for index in range(7):
    for value in ('', '-1', '1.5', 'abc', '2147483648', '999999999999999999999', ' 1'):
        args = valid.copy()
        args[index] = value
        invalid.append(args)
invalid.extend([[0, *valid[1:]], [*valid[:-1], 'FIFO'], [*valid[:-1], 'lifo']])
for args in invalid:
    result = run(args)
    assert result.returncode != 0 and not result.stdout, args
for policy in ('fifo', 'edf'):
    for n in (2, 3, 4, 5, 20):
        validate([n, 2000, 10, 10, 10, 5, 10, policy])
    for _ in range(5):
        validate([1, 30, 10, 10, 10, 1, 10, policy], burnout=True)
        validate([2, 30, 100, 10, 10, 1, 10, policy], burnout=True)
    validate([4, 1000, 0, 0, 0, 5, 0, policy])
    result = run([1, 0, 0, 0, 0, 0, 0, policy])
    assert result.returncode == 0 and not result.stdout
for _ in range(3):
    validate([5, 300, 20, 20, 20, 10, 80, 'edf'])
validate([7, 300, 20, 20, 20, 10, 80, 'edf'])
validate([10, 325, 100, 100, 100, 10, 0, 'fifo'])
arbitration()
print('PASS: invalid inputs, phases, completion, cooldown, burnout timing, FIFO/EDF arbitration')
