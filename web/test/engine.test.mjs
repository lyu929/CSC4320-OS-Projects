import assert from 'node:assert/strict';
import { test } from 'node:test';

import { SplitMix64, makePolicy, parseWorkload, simulate } from '../engine.js';

const P = (pid, arrival, burst, priority = 0) => ({ pid, arrival, burst, priority });
const runs = (r) => r.timeline.filter((s) => s.kind === 'run').map((s) => [s.pid, s.start, s.end]);
const sim = (w, name, cfg = {}, cs = 0) => simulate(w, makePolicy(name, cfg), { contextSwitch: cs });

test('SRTF textbook example (Silberschatz 5.3.2)', () => {
  const r = sim([P(1, 0, 8), P(2, 1, 4), P(3, 2, 9), P(4, 3, 5)], 'srtf');
  assert.deepEqual(runs(r), [[1, 0, 1], [2, 1, 5], [4, 5, 10], [1, 10, 17], [3, 17, 26]]);
  assert.equal(r.summary.avg_waiting, 6.5);
});

test('course example: FCFS and RR(q=2) averages', () => {
  const w = parseWorkload('PID Arrival Burst Priority\n1 0 7 2\n2 1 4 1\n3 3 6 3\n4 5 3 2\n5 7 5 1\n');
  assert.ok(Math.abs(sim(w, 'fcfs').summary.avg_waiting - 7.8) < 1e-12);
  const rr = sim(w, 'rr', { quantum: 2 });
  assert.ok(Math.abs(rr.summary.avg_waiting - 11.4) < 1e-12);
  assert.ok(Math.abs(rr.summary.avg_turnaround - 16.4) < 1e-12);
});

test('idle gaps and context switches appear on the timeline', () => {
  const r = sim([P(1, 3, 2), P(2, 9, 1), P(3, 9, 1)], 'fcfs', {}, 1);
  assert.deepEqual(r.timeline.map((s) => s.kind), ['idle', 'run', 'idle', 'switch', 'run', 'switch', 'run']);
});

test('invalid input is rejected with a line number', () => {
  assert.throws(() => parseWorkload('1 0 5 1\n1 2 3 1\n'), /line 2: duplicate PID 1/);
  assert.throws(() => parseWorkload('1 0 0 1\n'), /burst time must be > 0/);
  assert.throws(() => makePolicy('rr', { quantum: 0 }), /quantum must be positive/);
  assert.throws(() => makePolicy('lottery'), /unknown policy/);
});

test('SplitMix64 reference values', () => {
  const rng = new SplitMix64(0);
  assert.equal(rng.next(), 0xe220a8397b1dcdafn);
  assert.equal(rng.next(), 0x6e789e6aa1b965f4n);
});
