// JavaScript port of the C++ scheduling engine (scheduler/src/engine.cpp, policies.cpp).
// Semantics, tie-breaking and metrics are identical; web/test/crosscheck.mjs verifies
// this against JSON produced by the C++ `schedsim` on the committed golden workloads.

export const NEVER = Number.MAX_SAFE_INTEGER;

// ------------------------------------------------------------------ helpers
class MinHeap {
  constructor(less) {
    this.a = [];
    this.less = less;
  }
  get size() {
    return this.a.length;
  }
  push(x) {
    const a = this.a;
    a.push(x);
    let i = a.length - 1;
    while (i > 0) {
      const p = (i - 1) >> 1;
      if (!this.less(a[i], a[p])) break;
      [a[i], a[p]] = [a[p], a[i]];
      i = p;
    }
  }
  pop() {
    const a = this.a;
    if (!a.length) return null;
    const top = a[0];
    const last = a.pop();
    if (a.length) {
      a[0] = last;
      let i = 0;
      for (;;) {
        const l = 2 * i + 1;
        const r = l + 1;
        let m = i;
        if (l < a.length && this.less(a[l], a[m])) m = l;
        if (r < a.length && this.less(a[r], a[m])) m = r;
        if (m === i) break;
        [a[i], a[m]] = [a[m], a[i]];
        i = m;
      }
    }
    return top;
  }
}

// Entries are [key, arrival, pid, job]; lexicographic order like std::tuple in C++.
const lexLess = (x, y) => (x[0] !== y[0] ? x[0] < y[0] : x[1] !== y[1] ? x[1] < y[1] : x[2] < y[2]);

class KeyedQueue {
  constructor(keyFn) {
    this.heap = new MinHeap(lexLess);
    this.keyFn = keyFn;
  }
  push(j) {
    this.heap.push([this.keyFn(j), j.arrival, j.pid, j]);
  }
  pop() {
    const e = this.heap.pop();
    return e ? e[3] : null;
  }
  get empty() {
    return this.heap.size === 0;
  }
}

// ------------------------------------------------------------------ policies
class Policy {
  timeSlice() {
    return NEVER;
  }
  get preemptive() {
    return false;
  }
  shouldPreempt() {
    return false;
  }
  onSliceExpired() {}
  onDispatch() {}
  onRan() {}
  nextEvent() {
    return NEVER;
  }
  onEvent() {
    return false;
  }
}

class Fifo extends Policy {
  constructor(quantum = NEVER) {
    super();
    this.quantum = quantum;
    this.q = [];
    this.head = 0;
  }
  get name() {
    return this.quantum === NEVER ? 'FCFS' : `RR(q=${this.quantum})`;
  }
  add(j) {
    this.q.push(j);
  }
  pop() {
    if (this.head >= this.q.length) return null;
    const j = this.q[this.head++];
    if (this.head > 1024 && this.head * 2 > this.q.length) {
      this.q = this.q.slice(this.head);
      this.head = 0;
    }
    return j;
  }
  get empty() {
    return this.head >= this.q.length;
  }
  timeSlice() {
    return this.quantum;
  }
}

class ShortestJob extends Policy {
  constructor(preemptive) {
    super();
    this.pre = preemptive;
    this.q = new KeyedQueue(preemptive ? (j) => j.remaining : (j) => j.burst);
  }
  get name() {
    return this.pre ? 'SRTF' : 'SJF';
  }
  add(j) {
    this.q.push(j);
  }
  pop() {
    return this.q.pop();
  }
  get empty() {
    return this.q.empty;
  }
  get preemptive() {
    return this.pre;
  }
  shouldPreempt(running, arrived) {
    return arrived.remaining < running.remaining;
  }
}

function fmtNumber(x) {
  // matches std::ostream default formatting for the values used in names
  return Number.isInteger(x) ? String(x) : String(+x.toPrecision(6));
}

class PriorityPolicy extends Policy {
  constructor(preemptive, aging) {
    super();
    this.pre = preemptive;
    this.aging = aging;
    this.q = new KeyedQueue((j) => this.staticKey(j));
  }
  get name() {
    const n = this.pre ? 'Priority-P' : 'Priority';
    return this.aging > 0 ? `${n}(aging=${fmtNumber(this.aging)})` : n;
  }
  staticKey(j) {
    return this.aging <= 0 ? j.priority : j.priority - (j.waited - j.readySince) / this.aging;
  }
  effective(j, now) {
    return this.aging <= 0 ? j.priority : j.priority - (j.waited + (now - j.readySince)) / this.aging;
  }
  add(j) {
    this.q.push(j);
  }
  pop(now) {
    const j = this.q.pop();
    if (j) j.dispatchPriority = this.effective(j, now);
    return j;
  }
  get empty() {
    return this.q.empty;
  }
  get preemptive() {
    return this.pre;
  }
  shouldPreempt(running, arrived, now) {
    return this.effective(arrived, now) < running.dispatchPriority;
  }
}

class Hrrn extends Policy {
  constructor() {
    super();
    this.ready = [];
  }
  get name() {
    return 'HRRN';
  }
  add(j) {
    this.ready.push(j);
  }
  pop(now) {
    if (!this.ready.length) return null;
    const ratio = (j) => (now - j.arrival + j.burst) / j.burst;
    let best = 0;
    for (let i = 1; i < this.ready.length; i++) {
      const a = this.ready[best];
      const b = this.ready[i];
      const ra = ratio(a);
      const rb = ratio(b);
      const bBetter = ra !== rb ? rb > ra : b.arrival !== a.arrival ? b.arrival < a.arrival : b.pid < a.pid;
      if (bBetter) best = i;
    }
    return this.ready.splice(best, 1)[0];
  }
  get empty() {
    return this.ready.length === 0;
  }
}

class Mlfq extends Policy {
  constructor(quanta, boost) {
    super();
    this.quanta = quanta;
    this.boost = boost;
    this.lastBoost = 0;
    this.levels = quanta.map(() => []);
  }
  get name() {
    return `MLFQ(q=${this.quanta.join('/')}${this.boost > 0 ? `,boost=${this.boost}` : ''})`;
  }
  add(j) {
    this.levels[j.level].push(j);
  }
  pop() {
    for (const q of this.levels) if (q.length) return q.shift();
    return null;
  }
  get empty() {
    return this.levels.every((q) => q.length === 0);
  }
  timeSlice(j) {
    return this.quanta[j.level] - j.usedAtLevel;
  }
  get preemptive() {
    return true;
  }
  shouldPreempt(running, arrived) {
    return arrived.level < running.level;
  }
  onRan(j, ran) {
    j.usedAtLevel += ran;
  }
  onSliceExpired(j) {
    if (j.level + 1 < this.levels.length) j.level++;
    j.usedAtLevel = 0;
  }
  nextEvent() {
    return this.boost > 0 ? this.lastBoost + this.boost : NEVER;
  }
  onEvent(now, running) {
    this.lastBoost = now - (now % this.boost);
    const all = [];
    for (const q of this.levels) {
      for (const j of q) {
        j.level = 0;
        j.usedAtLevel = 0;
        all.push(j);
      }
      q.length = 0;
    }
    this.levels[0] = all;
    if (running) {
      running.level = 0;
      running.usedAtLevel = 0;
      return true;
    }
    return false;
  }
}

export const POLICY_NAMES = ['fcfs', 'sjf', 'srtf', 'hrrn', 'prio', 'prio-p', 'rr', 'mlfq'];

export function makePolicy(rawName, cfg = {}) {
  const c = { quantum: 2, mlfqQuanta: [2, 4, 8], mlfqBoost: 0, aging: 0, ...cfg };
  const name = rawName.toLowerCase();
  if (name === 'fcfs') return new Fifo();
  if (name === 'rr') {
    if (!(c.quantum > 0)) throw new Error('RR quantum must be positive');
    return new Fifo(c.quantum);
  }
  if (name === 'sjf') return new ShortestJob(false);
  if (name === 'srtf') return new ShortestJob(true);
  if (name === 'hrrn') return new Hrrn();
  if (['prio', 'priority', 'prio-p', 'priority-p'].includes(name)) {
    if (c.aging < 0) throw new Error('aging must be >= 0');
    return new PriorityPolicy(name.endsWith('p'), c.aging);
  }
  if (name === 'mlfq') {
    if (!c.mlfqQuanta.length) throw new Error('MLFQ needs at least one level');
    if (c.mlfqQuanta.some((q) => !(q > 0))) throw new Error('MLFQ quanta must be positive');
    if (c.mlfqBoost < 0) throw new Error('MLFQ boost period must be >= 0');
    return new Mlfq([...c.mlfqQuanta], c.mlfqBoost);
  }
  throw new Error(`unknown policy '${rawName}'`);
}

// ------------------------------------------------------------------ engine
export function validate(processes) {
  if (!processes.length) throw new Error('workload is empty');
  const seen = new Set();
  for (const p of processes) {
    if (seen.has(p.pid)) throw new Error(`duplicate PID ${p.pid}`);
    seen.add(p.pid);
    if (p.arrival < 0) throw new Error(`process ${p.pid}: arrival time must be >= 0`);
    if (p.burst <= 0) throw new Error(`process ${p.pid}: burst time must be > 0`);
  }
}

export function simulate(processes, policy, { contextSwitch = 0 } = {}) {
  validate(processes);
  if (contextSwitch < 0) throw new Error('context-switch cost must be >= 0');
  const jobs = processes.map((p) => ({
    pid: p.pid,
    arrival: p.arrival,
    burst: p.burst,
    priority: p.priority ?? 0,
    remaining: p.burst,
    firstRun: -1,
    completion: -1,
    readySince: 0,
    waited: 0,
    dispatchPriority: 0,
    level: 0,
    usedAtLevel: 0,
  }));
  const order = [...jobs].sort((a, b) => a.arrival - b.arrival || a.pid - b.pid);
  const timeline = [];
  const n = jobs.length;
  let next = 0;
  let done = 0;
  let now = 0;
  let running = null;
  let expired = null;
  let sliceEnd = NEVER;
  let lastPid = -1;
  let lastRunPid = -1;
  let dispatches = 0;
  let switches = 0;
  let fresh = false;
  let arrived = [];

  const pushSlice = (kind, pid, start, end) => {
    if (end <= start) return;
    if (kind === 'run') {
      if (lastRunPid !== -1 && pid !== lastRunPid) switches++;
      lastRunPid = pid;
    }
    const last = timeline[timeline.length - 1];
    if (!fresh && last && last.kind === kind && last.pid === pid && last.end === start) last.end = end;
    else timeline.push({ kind, pid, start, end });
    fresh = false;
  };
  const admit = () => {
    arrived = [];
    while (next < n && order[next].arrival <= now) {
      const j = order[next++];
      j.readySince = j.arrival;
      policy.add(j, j.arrival);
      arrived.push(j);
    }
  };
  const requeue = (j) => {
    j.readySince = now;
    policy.add(j, now);
  };

  while (done < n) {
    admit();
    if (expired) {
      policy.onSliceExpired(expired, now);
      requeue(expired);
      expired = null;
    }
    while (policy.nextEvent(now) <= now) {
      if (policy.onEvent(now, running) && running) {
        requeue(running);
        running = null;
      }
    }
    if (running && policy.preemptive) {
      for (const a of arrived) {
        if (policy.shouldPreempt(running, a, now)) {
          requeue(running);
          running = null;
          break;
        }
      }
    }
    if (!running) {
      if (policy.empty) {
        const t = order[next].arrival;
        pushSlice('idle', -1, now, t);
        now = t;
        continue;
      }
      const j = policy.pop(now);
      dispatches++;
      if (lastPid !== -1 && j.pid !== lastPid && contextSwitch > 0) {
        fresh = true;
        pushSlice('switch', -1, now, now + contextSwitch);
        now += contextSwitch;
      }
      j.waited += now - j.readySince;
      if (j.firstRun < 0) j.firstRun = now;
      policy.onDispatch(j, now);
      const slice = policy.timeSlice(j);
      sliceEnd = slice >= NEVER ? NEVER : now + slice;
      running = j;
      lastPid = j.pid;
      fresh = true;
    }
    let tNext = Math.min(now + running.remaining, sliceEnd);
    if (policy.preemptive && next < n) tNext = Math.min(tNext, order[next].arrival);
    tNext = Math.min(tNext, policy.nextEvent(now));
    if (tNext <= now) continue;
    const ran = tNext - now;
    pushSlice('run', running.pid, now, tNext);
    running.remaining -= ran;
    policy.onRan(running, ran);
    now = tNext;
    if (running.remaining === 0) {
      running.completion = now;
      done++;
      running = null;
      sliceEnd = NEVER;
    } else if (now >= sliceEnd) {
      expired = running;
      running = null;
      sliceEnd = NEVER;
    }
  }

  const stats = jobs
    .map((j) => ({
      pid: j.pid,
      arrival: j.arrival,
      burst: j.burst,
      priority: j.priority,
      start: j.firstRun,
      completion: j.completion,
      turnaround: j.completion - j.arrival,
      waiting: j.completion - j.arrival - j.burst,
      response: j.firstRun - j.arrival,
    }))
    .sort((a, b) => a.pid - b.pid);
  return { algorithm: policy.name, timeline, processes: stats, summary: summarize(stats, timeline, dispatches, switches) };
}

export function summarize(stats, timeline, dispatches, switches) {
  const n = stats.length;
  const s = {
    avg_waiting: 0,
    avg_turnaround: 0,
    avg_response: 0,
    avg_slowdown: 0,
    max_waiting: 0,
    makespan: 0,
    busy: 0,
    cpu_utilization: 0,
    throughput: 0,
    dispatches,
    context_switches: switches,
    fairness: 0,
  };
  let first = NEVER;
  let sx = 0;
  let sx2 = 0;
  for (const p of stats) {
    s.avg_waiting += p.waiting / n;
    s.avg_turnaround += p.turnaround / n;
    s.avg_response += p.response / n;
    s.avg_slowdown += p.turnaround / p.burst / n;
    s.max_waiting = Math.max(s.max_waiting, p.waiting);
    s.makespan = Math.max(s.makespan, p.completion);
    first = Math.min(first, p.arrival);
    const x = p.burst / p.turnaround;
    sx += x;
    sx2 += x * x;
  }
  for (const sl of timeline) if (sl.kind === 'run') s.busy += sl.end - sl.start;
  const span = s.makespan - first;
  s.cpu_utilization = span > 0 ? s.busy / span : 1;
  s.throughput = span > 0 ? n / span : 0;
  s.fairness = sx2 > 0 ? (sx * sx) / (n * sx2) : 1;
  return s;
}

// ------------------------------------------------------------------ workload I/O
export function parseWorkload(text) {
  const out = [];
  const seen = new Map();
  let seenContent = false;
  const lines = text.split(/\r?\n/);
  lines.forEach((raw, idx) => {
    const line = raw.replace(/#.*/, '').replace(/,/g, ' ').trim();
    if (!line) return;
    const f = line.split(/\s+/);
    const isInt = (t) => /^[+-]?\d+$/.test(t);
    if (!seenContent && !isInt(f[0])) {
      seenContent = true;
      return;
    }
    seenContent = true;
    const where = `line ${idx + 1}`;
    if (f.length < 3 || f.length > 4) throw new Error(`${where}: expected 'PID Arrival Burst [Priority]'`);
    const names = ['PID', 'arrival', 'burst', 'priority'];
    const v = f.map((t, i) => {
      if (!isInt(t)) throw new Error(`${where}: ${names[i]} is not an integer: '${t}'`);
      return parseInt(t, 10);
    });
    if (v[1] < 0) throw new Error(`${where}: arrival time must be >= 0`);
    if (v[2] <= 0) throw new Error(`${where}: burst time must be > 0`);
    if (seen.has(v[0])) throw new Error(`${where}: duplicate PID ${v[0]} (first on line ${seen.get(v[0])})`);
    seen.set(v[0], idx + 1);
    out.push({ pid: v[0], arrival: v[1], burst: v[2], priority: v[3] ?? 0 });
  });
  if (!out.length) throw new Error('no processes found');
  return out;
}

export function formatWorkload(processes) {
  return ['PID Arrival Burst Priority', ...processes.map((p) => `${p.pid} ${p.arrival} ${p.burst} ${p.priority}`)].join('\n');
}

// SplitMix64 + inverse-transform sampling: same seed -> same workload as `schedsim gen`.
export class SplitMix64 {
  constructor(seed) {
    this.state = BigInt.asUintN(64, BigInt(seed));
  }
  next() {
    this.state = BigInt.asUintN(64, this.state + 0x9e3779b97f4a7c15n);
    let z = this.state;
    z = BigInt.asUintN(64, (z ^ (z >> 30n)) * 0xbf58476d1ce4e5b9n);
    z = BigInt.asUintN(64, (z ^ (z >> 27n)) * 0x94d049bb133111ebn);
    return z ^ (z >> 31n);
  }
  uniform() {
    return Number(this.next() >> 11n) * 2 ** -53;
  }
}

export function generateWorkload({ count = 100, load = 0.8, meanBurst = 10, dist = 'exp', shortFraction = 0.9, levels = 5, seed = 1 } = {}) {
  if (count <= 0) throw new Error('count must be positive');
  const rng = new SplitMix64(seed);
  const expo = (mean) => -mean * Math.log1p(-rng.uniform());
  const rate = load / meanBurst;
  const shortMean = meanBurst / 4;
  const longMean = (meanBurst - shortFraction * shortMean) / (1 - shortFraction);
  const out = [];
  let clock = 0;
  for (let i = 0; i < count; i++) {
    if (i > 0) clock += expo(1 / rate);
    let b;
    if (dist === 'exp') b = expo(meanBurst);
    else if (dist === 'uniform') b = 1 + rng.uniform() * (2 * meanBurst - 2);
    else if (dist === 'bimodal') b = rng.uniform() < shortFraction ? expo(shortMean) : expo(longMean);
    else throw new Error(`unknown burst distribution '${dist}'`);
    const prio = 1 + Math.floor(rng.uniform() * levels);
    out.push({ pid: i + 1, arrival: Math.floor(clock), burst: Math.max(1, roundHalfAway(b)), priority: prio });
  }
  return out;
}

// std::llround rounds halfway cases away from zero (Math.round does not for negatives; bursts are positive).
function roundHalfAway(x) {
  return Math.sign(x) * Math.round(Math.abs(x));
}
