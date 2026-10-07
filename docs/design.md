# Design notes

## Scheduling engine (`scheduler/`)

**Event-driven time.** `simulate()` jumps from event to event: arrival, completion, slice expiry and
policy events such as an MLFQ boost. The cost therefore depends on the number of events, not on burst
lengths. A million processes take under a second (see [benchmarks.md](benchmarks.md)).

**Policy interface.** A `Policy` owns the ready queue. The engine asks it:

* which job runs next (`pop`);
* how long that job may run (`time_slice`);
* whether an arriving job preempts the running one (`should_preempt`).

Keeping this split means the eight policies contain no timekeeping code, and every metric is computed
in one place.

| policy | ready queue | pop | preemptive |
|---|---|---|---|
| FCFS / RR | deque | O(1) | no (RR: slice expiry) |
| SJF / SRTF | binary heap on (burst / remaining, arrival, pid) | O(log n) | SRTF on arrival |
| Priority (± aging) | binary heap, see below | O(log n) | `prio-p` on arrival |
| HRRN | vector, scanned | O(n) | no |
| MLFQ | one deque per level | O(levels) | when a higher-level job arrives |

**Aging without re-sorting.** The effective priority of a waiting job at time *t* is
`p − (w + t − r)/A`, where *w* is the time already waited, *r* is when the job entered the queue and
*A* is the aging interval. That equals `(p − (w − r)/A) − t/A`. The second term is the same for every
job, so the order between waiting jobs never changes, and the heap can be keyed on the constant part.

**Conventions.** These are the textbook conventions, and the course program used the same ones.

* A job that arrives at the same instant a slice expires is queued *before* the expired job.
* Ties are broken by arrival time, then by PID.
* SRTF does not preempt when the remaining times are equal.
* A context switch costs `--cs` time units and is charged to the incoming job's waiting time.
* Switching back to the same job is free.

**MLFQ** follows OSTEP's rules:

* new jobs start at the top level, and higher levels always run first;
* a job is demoted once it has used its whole allotment at a level, counted across preemptions, so it
  cannot game the scheduler by yielding just before the quantum ends;
* the last level is round robin;
* an optional periodic boost moves every job back to the top.

**Determinism.** The workload generator uses SplitMix64 with inverse-transform sampling rather than
`<random>` distributions, whose output is implementation-defined. The same seed gives the same workload
with every compiler, and the JavaScript port reproduces it too.

## Verification

Each layer is checked differently:

* **Textbook examples.** Gantt charts and averages from Silberschatz §5.3 and Stallings Table 9.5
  (FCFS, SJF, SRTF, HRRN).
* **Golden output.** `schedsim --format legacy` matches the course program's `sample_output.txt`
  byte for byte.
* **Invariants.** Every policy, with and without switch overhead, is run on 60 random workloads,
  including overloaded ones. Each run is checked for:
  * a contiguous timeline;
  * no job running before it arrives;
  * work conservation (the CPU is never idle while a job is ready);
  * every job getting exactly its burst;
  * the metric identities (TAT = CT − AT, WT = TAT − BT, RT ≤ WT);
  * a context-switch count that matches the timeline.
* **Optimality.** On 100 random workloads, SRTF's mean turnaround is at most that of every other
  policy.
* **Sanitizers.** The whole suite also runs under AddressSanitizer and UndefinedBehaviorSanitizer.
* **Cross-language check.** `web/engine.js` must reproduce the C++ JSON output (timeline,
  per-process statistics and summary) for 20 golden cases, and must regenerate the same random
  workloads.

## Synchronization library (`sync/`)

Every primitive is checked through an invariant, not through printed output:

* **Bounded buffers.**
  * Occupancy never exceeds the capacity (enforced inside the critical section).
  * Multi-producer/multi-consumer runs lose and duplicate nothing.
  * Each consumer sees every producer's items in FIFO order.
  * Interrupting a blocked producer leaks no slot or permit.
* **Readers–writers.** The harness counts active readers and writers on entry and detects torn reads
  through a version/value pair. Every policy must report zero violations. A starvation scenario shows
  that reader preference makes the writer wait more than three times longer than writer preference or
  the fair lock.
* **Dining philosophers.**
  * Neighbours never eat at the same time.
  * The naive strategy is driven into a guaranteed deadlock: a barrier is placed after the first fork
    is taken, and a watchdog detects the deadlock and recovers.
  * The ordered, waiter and try-lock strategies always finish.

Timing-dependent assertions use only one-sided bounds or large ratios, so the tests are not flaky on
slow CI machines.
