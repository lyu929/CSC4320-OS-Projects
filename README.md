# CSC 4320 Operating Systems Projects (Spring 2026)

Course projects for CSC 4320 Operating Systems at Georgia State University.

## Project 1: CPU Scheduling Simulation (C++)

`project1-cpu-scheduling/` reads processes (PID, arrival time, burst time, priority) from `processes.txt` and simulates two CPU scheduling algorithms:

- **First-Come, First-Served (FCFS)**: non-preemptive, ordered by arrival time
- **Round Robin (RR)**: preemptive, time quantum = 2

For each algorithm the program prints a text Gantt chart, per-process waiting time (WT), turnaround time (TAT) and completion time (CT), and the average WT and TAT.

```bash
cd project1-cpu-scheduling
g++ -std=c++17 -o scheduler scheduler.cpp
./scheduler
```

See `sample_output.txt` for an example run.

## Project 2: Thread-Based Process Simulation and Synchronization (Java)

`project2-threads-sync/` has two parts:

1. **Process simulation**: each process in `processes.txt` runs as a thread (`ProcessThread`), and `Thread.sleep()` stands in for its CPU burst time.
2. **Producer-Consumer problem**: one producer and multiple consumers share a bounded buffer (`BoundedBuffer`). Three semaphores (`empty`, `full`, `mutex`) keep the buffer from overflowing or underflowing and allow only one thread into it at a time.

```bash
cd project2-threads-sync
javac *.java
java Main
```

See `sample_output.txt` for an example run.
