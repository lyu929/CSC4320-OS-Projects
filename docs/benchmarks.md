# Benchmarks

Generated with `build/release/scheduler/schedbench` (GCC 13, -O3 Release; one core).
Workloads: Poisson arrivals at the given offered load, 400 processes, mean burst 10, 20 seeds each.
RR quantum 4, MLFQ quanta 4/8/16 with a boost every 400 units, priority aging 1 level / 50 units.

## Exponential bursts

### load 0.5

| policy | avg waiting | avg response | avg slowdown | p95 slowdown | Jain fairness | sim time (us) |
|---|---|---|---|---|---|---|
| FCFS | 9.29 ± 2.23 | 9.29 | 3.61 | 14.48 | 0.794 | 67.60 |
| SJF | 6.54 ± 1.21 | 6.54 | 2.37 | 7.70 | 0.840 | 271.53 |
| SRTF | 4.05 ± 0.70 | 2.54 | 1.23 | 2.14 | 0.953 | 75.05 |
| HRRN | 7.37 ± 1.53 | 7.37 | 2.61 | 8.73 | 0.813 | 66.94 |
| Priority(aging=50) | 9.04 ± 2.17 | 9.04 | 3.46 | 13.65 | 0.810 | 74.66 |
| Priority-P(aging=50) | 8.99 ± 2.18 | 6.51 | 2.93 | 10.54 | 0.843 | 73.84 |
| RR(q=4) | 8.99 ± 1.92 | 2.28 | 2.08 | 5.16 | 0.837 | 87.67 |
| MLFQ(q=4/8/16,boost=400) | 8.82 ± 1.72 | 0.44 | 1.64 | 3.81 | 0.886 | 100.22 |

### load 0.8

| policy | avg waiting | avg response | avg slowdown | p95 slowdown | Jain fairness | sim time (us) |
|---|---|---|---|---|---|---|
| FCFS | 34.09 ± 11.62 | 34.09 | 10.88 | 47.43 | 0.563 | 69.12 |
| SJF | 16.77 ± 3.84 | 16.77 | 3.56 | 11.31 | 0.733 | 71.92 |
| SRTF | 12.09 ± 3.22 | 8.64 | 1.57 | 3.36 | 0.905 | 79.37 |
| HRRN | 22.15 ± 6.24 | 22.15 | 4.86 | 13.63 | 0.627 | 76.25 |
| Priority(aging=50) | 33.70 ± 11.05 | 33.70 | 10.54 | 46.65 | 0.614 | 80.18 |
| Priority-P(aging=50) | 33.48 ± 11.01 | 29.15 | 9.59 | 44.14 | 0.658 | 88.95 |
| RR(q=4) | 32.95 ± 11.04 | 10.34 | 5.49 | 15.96 | 0.620 | 94.04 |
| MLFQ(q=4/8/16,boost=400) | 32.42 ± 10.54 | 1.21 | 3.17 | 9.83 | 0.744 | 103.07 |

### load 0.95

| policy | avg waiting | avg response | avg slowdown | p95 slowdown | Jain fairness | sim time (us) |
|---|---|---|---|---|---|---|
| FCFS | 92.40 ± 53.44 | 92.40 | 27.43 | 123.77 | 0.392 | 74.35 |
| SJF | 33.01 ± 13.93 | 33.01 | 4.37 | 13.73 | 0.700 | 76.53 |
| SRTF | 27.09 ± 13.78 | 21.96 | 2.04 | 5.42 | 0.872 | 91.81 |
| HRRN | 52.68 ± 27.77 | 52.68 | 8.11 | 18.01 | 0.500 | 91.26 |
| Priority(aging=50) | 91.91 ± 52.70 | 91.91 | 27.04 | 120.99 | 0.437 | 87.48 |
| Priority-P(aging=50) | 91.39 ± 52.83 | 88.02 | 26.13 | 120.17 | 0.457 | 100.23 |
| RR(q=4) | 89.40 ± 52.66 | 29.70 | 13.43 | 40.28 | 0.449 | 144.73 |
| MLFQ(q=4/8/16,boost=400) | 87.91 ± 52.60 | 4.16 | 7.29 | 21.70 | 0.586 | 111.19 |

## Bimodal bursts (90 % short, mean 2.5; 10 % long, mean 77.5)

### load 0.8

| policy | avg waiting | avg response | avg slowdown | p95 slowdown | Jain fairness | sim time (us) |
|---|---|---|---|---|---|---|
| FCFS | 143.63 ± 86.30 | 143.63 | 82.41 | 364.27 | 0.371 | 75.99 |
| SJF | 52.63 ± 15.14 | 52.63 | 26.20 | 120.47 | 0.490 | 77.51 |
| SRTF | 9.33 ± 6.33 | 5.36 | 1.17 | 1.99 | 0.966 | 96.90 |
| HRRN | 58.26 ± 18.55 | 58.26 | 28.41 | 118.20 | 0.418 | 92.79 |
| Priority(aging=50) | 143.13 ± 87.67 | 143.13 | 82.53 | 377.07 | 0.386 | 89.82 |
| Priority-P(aging=50) | 127.80 ± 91.05 | 125.42 | 72.08 | 357.18 | 0.478 | 95.65 |
| RR(q=4) | 29.62 ± 22.81 | 8.52 | 6.33 | 20.82 | 0.607 | 114.63 |
| MLFQ(q=4/8/16,boost=400) | 21.85 ± 18.31 | 0.62 | 1.72 | 5.16 | 0.897 | 101.27 |

## Engine scaling (load 0.9, exponential bursts)

| processes | policy | sim time (ms) | ns per process |
|---|---|---|---|
| 1000 | FCFS | 0.3 | 265 |
| 1000 | SRTF | 0.3 | 251 |
| 1000 | Priority(aging=50) | 0.2 | 234 |
| 1000 | RR(q=4) | 0.3 | 288 |
| 1000 | MLFQ(q=4/8/16,boost=400) | 1.4 | 1380 |
| 10000 | FCFS | 3.4 | 341 |
| 10000 | SRTF | 3.8 | 379 |
| 10000 | Priority(aging=50) | 3.6 | 360 |
| 10000 | RR(q=4) | 4.0 | 397 |
| 10000 | MLFQ(q=4/8/16,boost=400) | 4.1 | 411 |
| 100000 | FCFS | 50.7 | 507 |
| 100000 | SRTF | 66.3 | 663 |
| 100000 | Priority(aging=50) | 59.0 | 590 |
| 100000 | RR(q=4) | 68.1 | 681 |
| 100000 | MLFQ(q=4/8/16,boost=400) | 49.4 | 494 |
| 1000000 | FCFS | 673.9 | 674 |
| 1000000 | SRTF | 683.3 | 683 |
| 1000000 | Priority(aging=50) | 696.4 | 696 |
| 1000000 | RR(q=4) | 750.8 | 751 |
| 1000000 | MLFQ(q=4/8/16,boost=400) | 784.0 | 784 |
