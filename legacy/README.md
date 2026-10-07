# Original course submissions (CSC 4320, Spring 2026)

These folders are the projects exactly as submitted. They are kept for reference and still build:

```bash
cd legacy/project1-cpu-scheduling && g++ -std=c++17 -o scheduler scheduler.cpp && ./scheduler
cd legacy/project2-threads-sync && javac *.java && java Main
```

The maintained versions live in [`../scheduler`](../scheduler) (C++20 engine with eight policies),
[`../sync`](../sync) (Java synchronization library) and [`../web`](../web) (browser visualizer).
`schedsim --format legacy` reproduces `project1-cpu-scheduling/sample_output.txt` byte for byte, and
a unit test checks this.

## Issues fixed in the rewrite

| Original behaviour | Now |
|---|---|
| Only FCFS and RR; the priority column was read but never used | FCFS, SJF, SRTF, HRRN, priority (non-preemptive and preemptive, with aging), RR, MLFQ (demotion, boost) |
| `quantum <= 0` made RR loop forever | rejected with an error |
| Idle CPU time was missing from the Gantt chart | idle and context-switch slices are part of the timeline |
| `exit(1)` on bad input, no line numbers | `ParseError` with file:line, duplicate-PID and range checks |
| Output only as text | text, legacy text, comparison table, JSON, CSV, SVG Gantt charts |
| `Thread.sleep(burst * 1000)`: 25 s for the sample | time unit is configurable (default 100 ms); optional CPU limit reproduces FCFS |
| `InterruptedException` swallowed | the interrupt flag is restored and the buffer is left consistent (no leaked permits) |
| Consumers hard-coded to take 5 items each (10 = 5 + 5) | poison-pill shutdown works for any producer/consumer counts |
| No tests | GoogleTest, JUnit 5 and node:test suites, sanitizers, CI |
