// The JavaScript engine must reproduce the C++ simulator exactly on the golden workloads.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { test } from 'node:test';
import { fileURLToPath } from 'node:url';

import { POLICY_NAMES, generateWorkload, makePolicy, parseWorkload, simulate } from '../engine.js';

const golden = join(dirname(fileURLToPath(import.meta.url)), '..', '..', 'tests', 'golden');
const manifest = JSON.parse(readFileSync(join(golden, 'manifest.json'), 'utf8'));

for (const c of manifest) {
  test(`matches C++ on ${c.expected}`, () => {
    const text = readFileSync(join(golden, c.workload), 'utf8');
    const workload = parseWorkload(text);
    const expected = JSON.parse(readFileSync(join(golden, c.expected), 'utf8'));
    assert.equal(expected.length, POLICY_NAMES.length);
    POLICY_NAMES.forEach((name, i) => {
      const got = simulate(workload, makePolicy(name, c.params), { contextSwitch: c.params.contextSwitch });
      const want = expected[i];
      assert.equal(got.algorithm, want.algorithm);
      assert.deepEqual(got.timeline, want.timeline, `${want.algorithm} timeline`);
      assert.deepEqual(got.processes, want.processes, `${want.algorithm} per-process stats`);
      for (const [k, v] of Object.entries(want.summary)) {
        assert.ok(Math.abs(got.summary[k] - v) <= 1e-8 * Math.max(1, Math.abs(v)), `${want.algorithm} ${k}: ${got.summary[k]} vs ${v}`);
      }
    });
  });

  if (c.generator) {
    test(`generator reproduces ${c.workload}`, () => {
      const text = readFileSync(join(golden, c.workload), 'utf8');
      assert.deepEqual(generateWorkload(c.generator), parseWorkload(text));
    });
  }
}
