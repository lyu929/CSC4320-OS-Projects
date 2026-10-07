import { POLICY_NAMES, formatWorkload, generateWorkload, makePolicy, parseWorkload, simulate } from './engine.js';

const EXAMPLE = `PID Arrival Burst Priority
1 0 7 2
2 1 4 1
3 3 6 3
4 5 3 2
5 7 5 1`;

const LABELS = {
  fcfs: 'FCFS', sjf: 'SJF', srtf: 'SRTF', hrrn: 'HRRN', prio: 'Priority', 'prio-p': 'Priority (preemptive)',
  rr: 'Round robin', mlfq: 'MLFQ',
};
const DEFAULT_ON = new Set(['fcfs', 'sjf', 'srtf', 'rr', 'mlfq']);
const $ = (id) => document.getElementById(id);
const SVG_NS = 'http://www.w3.org/2000/svg';

function el(tag, attrs = {}, text) {
  const e = document.createElementNS(SVG_NS, tag);
  for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
  if (text !== undefined) e.textContent = text;
  return e;
}

const color = (index) => `hsl(${(index * 137.508) % 360},65%,55%)`;
const fmt = (x, d = 2) => (Number.isInteger(x) ? String(x) : x.toFixed(d));

function niceStep(span, maxTicks = 20) {
  for (let mag = 1; ; mag *= 10) for (const m of [1, 2, 5]) if (span / (m * mag) <= maxTicks) return m * mag;
}

// ---------------------------------------------------------------- state <-> URL
function readParams() {
  const mlfq = $('mlfq').value.split('/').map(Number).filter((x) => x > 0);
  return {
    quantum: Number($('quantum').value),
    mlfqQuanta: mlfq.length ? mlfq : [2, 4, 8],
    mlfqBoost: Number($('boost').value),
    aging: Number($('aging').value),
    contextSwitch: Number($('cs').value),
  };
}

function selectedPolicies() {
  return [...document.querySelectorAll('#policies input:checked')].map((i) => i.value);
}

function saveToHash() {
  const p = readParams();
  const state = {
    w: $('workload').value,
    a: selectedPolicies().join(','),
    q: p.quantum,
    m: $('mlfq').value,
    b: p.mlfqBoost,
    g: p.aging,
    c: p.contextSwitch,
  };
  const encoded = btoa(unescape(encodeURIComponent(JSON.stringify(state))));
  history.replaceState(null, '', `#${encoded}`);
  return location.href;
}

function loadFromHash() {
  if (!location.hash) return false;
  try {
    const s = JSON.parse(decodeURIComponent(escape(atob(location.hash.slice(1)))));
    $('workload').value = s.w;
    const on = new Set(String(s.a).split(','));
    document.querySelectorAll('#policies input').forEach((i) => (i.checked = on.has(i.value)));
    $('quantum').value = s.q;
    $('mlfq').value = s.m;
    $('boost').value = s.b;
    $('aging').value = s.g;
    $('cs').value = s.c;
    return true;
  } catch {
    return false;
  }
}

// ---------------------------------------------------------------- rendering
function renderComparison(results) {
  const cols = [
    ['algorithm', 'Policy', null],
    ['avg_waiting', 'avg WT', 'min'],
    ['avg_turnaround', 'avg TAT', 'min'],
    ['avg_response', 'avg RT', 'min'],
    ['avg_slowdown', 'slowdown', 'min'],
    ['max_waiting', 'max WT', 'min'],
    ['context_switches', 'switches', 'min'],
    ['cpu_utilization', 'util %', 'max'],
    ['fairness', 'fairness', 'max'],
  ];
  const table = $('compare');
  table.replaceChildren();
  const head = table.createTHead().insertRow();
  const rows = results.map((r) => ({ algorithm: r.algorithm, ...r.summary }));
  let sortKey = 'avg_waiting';
  let asc = true;
  const draw = () => {
    const body = table.tBodies[0] ?? table.createTBody();
    body.replaceChildren();
    const sorted = [...rows].sort((x, y) => {
      const a = x[sortKey];
      const b = y[sortKey];
      return (typeof a === 'string' ? a.localeCompare(b) : a - b) * (asc ? 1 : -1);
    });
    for (const r of sorted) {
      const tr = body.insertRow();
      for (const [key, , best] of cols) {
        const td = tr.insertCell();
        const v = r[key];
        td.textContent = key === 'cpu_utilization' ? fmt(100 * v, 1) : typeof v === 'number' ? fmt(v, key === 'fairness' ? 3 : 2) : v;
        if (best && rows.length > 1) {
          const values = rows.map((x) => x[key]);
          const target = best === 'min' ? Math.min(...values) : Math.max(...values);
          if (Math.abs(v - target) < 1e-12) td.classList.add('best');
        }
      }
    }
  };
  for (const [key, label] of cols) {
    const th = document.createElement('th');
    th.textContent = label;
    th.title = 'sort';
    th.addEventListener('click', () => {
      asc = sortKey === key ? !asc : true;
      sortKey = key;
      draw();
    });
    head.appendChild(th);
  }
  draw();
}

function renderBarChart(results) {
  const metrics = [
    ['avg_waiting', 'avg waiting'],
    ['avg_turnaround', 'avg turnaround'],
    ['avg_response', 'avg response'],
  ];
  const W = 760;
  const rowH = 16;
  const groupH = metrics.length * rowH + 14;
  const left = 170;
  const H = results.length * groupH + 30;
  const max = Math.max(1, ...results.flatMap((r) => metrics.map(([k]) => r.summary[k])));
  const svg = el('svg', { viewBox: `0 0 ${W} ${H}`, role: 'img', 'aria-label': 'Average waiting, turnaround and response time per policy' });
  results.forEach((r, i) => {
    const y0 = 10 + i * groupH;
    svg.appendChild(el('text', { x: 0, y: y0 + groupH / 2, 'font-weight': '600' }, r.algorithm));
    metrics.forEach(([k], j) => {
      const w = ((W - left - 60) * r.summary[k]) / max;
      const y = y0 + j * rowH;
      const rect = el('rect', { x: left, y, width: Math.max(0.5, w), height: rowH - 3, fill: `hsl(${210 + j * 40},65%,${55 - j * 5}%)`, rx: 2 });
      rect.appendChild(el('title', {}, `${metrics[j][1]}: ${fmt(r.summary[k])}`));
      svg.appendChild(rect);
      svg.appendChild(el('text', { x: left + w + 4, y: y + rowH - 5 }, fmt(r.summary[k])));
    });
  });
  metrics.forEach(([, label], j) => {
    const x = left + j * 150;
    svg.appendChild(el('rect', { x, y: H - 14, width: 10, height: 10, fill: `hsl(${210 + j * 40},65%,${55 - j * 5}%)` }));
    svg.appendChild(el('text', { x: x + 14, y: H - 5 }, label));
  });
  $('chart').replaceChildren(svg);
}

function renderGantt(result) {
  const pids = result.processes.map((p) => p.pid);
  const rowOf = new Map(pids.map((p, i) => [p, i + 1]));
  const t1 = result.timeline.at(-1).end;
  const W = 900;
  const left = 46;
  const row = 22;
  const top = 6;
  const H = top + row * (pids.length + 1) + 34;
  const x = (t) => left + ((W - left - 16) * t) / Math.max(1, t1);
  const svg = el('svg', { viewBox: `0 0 ${W} ${H}`, role: 'img', 'aria-label': `Gantt chart for ${result.algorithm}` });
  svg.appendChild(el('text', { x: 4, y: top + row * 0.7, 'font-weight': '700' }, 'CPU'));
  pids.forEach((pid, i) => svg.appendChild(el('text', { x: 4, y: top + row * (i + 1.7) }, `P${pid}`)));
  for (const p of result.processes) {
    const y = top + row * rowOf.get(p.pid) + 9;
    svg.appendChild(el('rect', { x: x(p.arrival), y, width: x(p.completion) - x(p.arrival), height: 4, fill: 'var(--wait)' }));
  }
  for (const s of result.timeline) {
    const w = x(s.end) - x(s.start);
    const tip = `${s.kind === 'run' ? `P${s.pid}` : s.kind}: ${s.start}–${s.end}`;
    const fill = s.kind === 'run' ? color(rowOf.get(s.pid)) : s.kind === 'idle' ? 'var(--idle)' : 'var(--switch)';
    const cpu = el('rect', { x: x(s.start), y: top + 2, width: w, height: row - 4, fill });
    cpu.appendChild(el('title', {}, tip));
    svg.appendChild(cpu);
    if (s.kind === 'run') {
      const r = el('rect', { x: x(s.start), y: top + row * rowOf.get(s.pid) + 2, width: w, height: row - 4, fill, rx: 2 });
      r.appendChild(el('title', {}, tip));
      svg.appendChild(r);
      if (w > 22) svg.appendChild(el('text', { x: x(s.start) + w / 2, y: top + row * 0.7, 'text-anchor': 'middle', fill: '#fff' }, `P${s.pid}`));
    }
  }
  const axisY = top + row * (pids.length + 1) + 6;
  svg.appendChild(el('line', { x1: left, y1: axisY, x2: x(t1), y2: axisY, stroke: 'currentColor' }));
  const step = niceStep(t1);
  const ticks = [];
  for (let t = 0; t <= t1; t += step) ticks.push(t);
  if (t1 - ticks.at(-1) > step / 2) ticks.push(t1);
  for (const t of ticks) {
    svg.appendChild(el('line', { x1: x(t), y1: axisY, x2: x(t), y2: axisY + 4, stroke: 'currentColor' }));
    svg.appendChild(el('text', { x: x(t), y: axisY + 16, 'text-anchor': 'middle' }, String(t)));
  }

  const panel = document.createElement('div');
  panel.className = 'panel';
  const title = document.createElement('div');
  title.className = 'gantt-title';
  const h = document.createElement('h2');
  h.textContent = result.algorithm;
  const info = document.createElement('span');
  const s = result.summary;
  info.textContent = `avg WT ${fmt(s.avg_waiting)} · avg TAT ${fmt(s.avg_turnaround)} · avg RT ${fmt(s.avg_response)} · ${s.context_switches} switches`;
  title.append(h, info);
  panel.append(title, svg);

  const details = document.createElement('details');
  const sum = document.createElement('summary');
  sum.textContent = 'Per-process table';
  const wrap = document.createElement('div');
  wrap.className = 'table-wrap';
  const table = document.createElement('table');
  const head = table.createTHead().insertRow();
  for (const c of ['PID', 'AT', 'BT', 'PR', 'start', 'CT', 'TAT', 'WT', 'RT']) {
    const th = document.createElement('th');
    th.textContent = c;
    head.appendChild(th);
  }
  const body = table.createTBody();
  for (const p of result.processes) {
    const tr = body.insertRow();
    for (const v of [`P${p.pid}`, p.arrival, p.burst, p.priority, p.start, p.completion, p.turnaround, p.waiting, p.response]) {
      tr.insertCell().textContent = v;
    }
  }
  wrap.appendChild(table);
  details.append(sum, wrap);
  panel.appendChild(details);
  return panel;
}

// ---------------------------------------------------------------- actions
function run() {
  const errorBox = $('workload-error');
  errorBox.textContent = '';
  let workload;
  try {
    workload = parseWorkload($('workload').value);
  } catch (e) {
    errorBox.textContent = e.message;
    return;
  }
  const params = readParams();
  const results = [];
  for (const name of selectedPolicies()) {
    try {
      results.push(simulate(workload, makePolicy(name, params), { contextSwitch: params.contextSwitch }));
    } catch (e) {
      errorBox.textContent = `${LABELS[name]}: ${e.message}`;
      return;
    }
  }
  if (!results.length) {
    errorBox.textContent = 'Select at least one policy.';
    return;
  }
  renderComparison(results);
  renderBarChart(results);
  $('gantts').replaceChildren(...results.map(renderGantt));
  saveToHash();
}

function init() {
  const box = $('policies');
  for (const name of POLICY_NAMES) {
    const label = document.createElement('label');
    const input = document.createElement('input');
    input.type = 'checkbox';
    input.value = name;
    input.checked = DEFAULT_ON.has(name);
    label.append(input, LABELS[name]);
    box.appendChild(label);
  }
  if (!loadFromHash()) $('workload').value = EXAMPLE;
  $('example').addEventListener('click', () => {
    $('workload').value = EXAMPLE;
    run();
  });
  $('random').addEventListener('click', () => {
    const seed = Number($('gen-seed').value);
    const w = generateWorkload({
      count: Number($('gen-n').value),
      load: Number($('gen-load').value),
      meanBurst: Number($('gen-mean').value),
      dist: $('gen-dist').value,
      seed,
    });
    $('workload').value = formatWorkload(w);
    $('gen-seed').value = seed + 1;
    run();
  });
  $('run').addEventListener('click', run);
  $('share').addEventListener('click', async () => {
    const url = saveToHash();
    try {
      await navigator.clipboard.writeText(url);
      $('share').textContent = 'Copied';
    } catch {
      $('share').textContent = 'Link in address bar';
    }
    setTimeout(() => ($('share').textContent = 'Copy link'), 1500);
  });
  document.querySelectorAll('.controls input, .controls select').forEach((i) => i.addEventListener('change', run));
  run();
}

init();
