# OS Simulators: CPU Scheduling and Thread Synchronization

[![CI](https://github.com/lyu929/CSC4320-OS-Projects/actions/workflows/ci.yml/badge.svg)](https://github.com/lyu929/CSC4320-OS-Projects/actions/workflows/ci.yml)

These started as two course projects for CSC 4320 Operating Systems (Georgia State University,
Spring 2026) and have since been extended into tested tools:

| Component | Language | What it is |
|---|---|---|
| [`scheduler/`](scheduler) | C++20, CMake | Event-driven CPU-scheduling engine with 8 policies, metrics, text/JSON/CSV/SVG output, a workload generator and benchmarks |
| [`web/`](web) | JavaScript (no dependencies) | In-browser visualizer: Gantt charts and policy comparison, using a port of the engine cross-checked against the C++ output |
| [`sync/`](sync) | Java 17, Maven | Bounded buffers (semaphore, monitor, lock + conditions), readers–writers (3 policies), dining philosophers (4 strategies) and a thread-per-process simulation |
| [`legacy/`](legacy) | C++ / Java | The original submissions, unchanged |

## Highlights

* **Policies.** FCFS, SJF, SRTF, HRRN, priority (non-preemptive and preemptive, with aging), round
  robin, and MLFQ (allotment-based demotion, periodic boost). Context-switch overhead and idle time are
  shown on the timeline.
* **Verified, not just printed.**
  * Textbook answers from Silberschatz and Stallings.
  * Byte-identical reproduction of the course program's output.
  * Invariants on 60 random workloads × every policy (work conservation, exact service, metric
    identities).
  * SRTF optimality checked on 100 workloads.
  * ASan/UBSan builds.
* **One engine, two languages.** The browser visualizer must reproduce the C++ JSON output exactly on
  20 golden cases, and regenerate the same random workloads (portable SplitMix64 generator).
* **Fast.** Simulating 1,000,000 processes takes under 0.8 s per policy, and cost grows roughly
  linearly ([benchmarks](docs/benchmarks.md)).
* **Concurrency tested by invariants.** No lost or duplicated items, a capacity bound, FIFO order and
  interrupt safety for every buffer. Zero exclusion violations for every readers–writers lock. A
  measurable writer-starvation effect. A guaranteed (and detected) deadlock for naive dining
  philosophers.

### What the benchmark shows

400 processes, offered load 0.8, mean of 20 random workloads:

| Policy | avg waiting (exp.) | avg response (exp.) | avg waiting (bimodal) | avg slowdown (bimodal) |
|---|---|---|---|---|
| FCFS | 34.1 | 34.1 | 143.6 | 82.4 |
| SJF | 16.8 | 16.8 | 52.6 | 26.2 |
| SRTF | **12.1** | 8.6 | **9.3** | **1.17** |
| RR (q = 4) | 33.0 | 10.3 | 29.6 | 6.33 |
| MLFQ (4/8/16) | 32.4 | **1.2** | 21.9 | 1.72 |

* SRTF is optimal for waiting time, but it has to know burst lengths in advance.
* With exponential bursts, RR's mean waiting time is no better than FCFS. This is the
  processor-sharing result: when bursts are memoryless, it does not pay to preempt.
* When bursts are highly variable (bimodal), RR and especially MLFQ get close to SRTF without knowing
  any burst lengths. MLFQ also gives by far the best response time.

## Quick start

```bash
# C++ engine (needs CMake >= 3.20 and a C++20 compiler; GoogleTest is fetched automatically)
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
build/dev/scheduler/schedsim -a all --format compare scheduler/data/processes.txt
build/dev/scheduler/schedsim -a srtf,rr,mlfq -q 4 --cs 1 --svg chart scheduler/data/processes.txt
build/dev/scheduler/schedsim gen -n 50 --seed 7 --load 0.9 --dist bimodal > workload.txt
build/dev/scheduler/schedsim --format legacy legacy/project1-cpu-scheduling/processes.txt 2

# Web visualizer (static files, no build step)
python3 -m http.server -d web 8080      # open http://localhost:8080
cd web && npm test                      # unit tests + cross-check against tests/golden

# Java synchronization library
cd sync && mvn -q verify
java -jar target/os-sync-1.0.0.jar prodcons --impl monitor --producers 3 --consumers 2 --items 20
java -jar target/os-sync-1.0.0.jar rw --policy writer
java -jar target/os-sync-1.0.0.jar dining --strategy naive --force-deadlock --timeout-ms 1000
java -jar target/os-sync-1.0.0.jar process --file ../scheduler/data/processes.txt --cpus 1 --unit-ms 50
```

Sanitizer build: `cmake --preset asan && cmake --build --preset asan && ctest --preset asan`.

### `schedsim` output

```
$ schedsim -a all --boost 20 --aging 4 --format compare processes.txt
algorithm                 avg WT  avg TAT   avg RT  slowdown  max WT   util%    CS fairness
FCFS                        7.80    12.80     7.80      2.89      13  100.00     4    0.728
SJF                         6.80    11.80     6.80      2.40      16  100.00     4    0.792
SRTF                        5.80    10.80     3.40      1.92      16  100.00     5    0.831
HRRN                        7.20    12.20     7.20      2.59      13  100.00     4    0.759
Priority(aging=4)           7.40    12.40     7.40      2.73      16  100.00     4    0.751
Priority-P(aging=4)         7.00    12.00     6.20      2.54      16  100.00     5    0.783
RR(q=2)                    11.40    16.40     3.20      3.37      15  100.00    13    0.942
MLFQ(q=2/4/8,boost=20)     13.00    18.00     0.80      3.79      17  100.00    11    0.964
```

## Repository layout

```
scheduler/
  include/sched/   types.hpp policy.hpp engine.hpp io.hpp workload.hpp
  src/             engine.cpp policies.cpp io.cpp workload.cpp
  apps/            schedsim.cpp (CLI), schedbench.cpp (benchmarks)
  tests/           GoogleTest: textbook cases, invariants, golden output, parser, generator
web/               index.html app.js engine.js style.css, test/ (node:test)
sync/              Maven project: buffer/ rw/ dining/ process/ trace/ + JUnit 5 tests
tests/golden/      C++ reference outputs used by the JavaScript cross-check
scripts/           make_golden.sh
docs/              design.md (architecture and conventions), benchmarks.md
legacy/            original course submissions
```

## Continuous integration

`.github/workflows/ci.yml` runs five jobs:

1. builds and tests the C++ engine with GCC and Clang (Debug, warnings as errors);
2. runs the suite under ASan/UBSan;
3. regenerates `tests/golden` and fails if it differs from the committed files;
4. runs the JavaScript tests;
5. runs `mvn verify` for the Java library.

`.github/workflows/pages.yml` publishes `web/` to GitHub Pages.

See [docs/design.md](docs/design.md) for the engine design, the scheduling conventions and the test
strategy.
