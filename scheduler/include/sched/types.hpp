// Core value types shared by the engine, the policies and the I/O layer.
#pragma once

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace sched {

using Time = std::int64_t;
inline constexpr Time kNever = std::numeric_limits<Time>::max() / 4;

/// One line of the workload file: what the user specifies.
struct Process {
    int pid = 0;
    Time arrival = 0;
    Time burst = 0;
    int priority = 0;  ///< lower value = more important (textbook convention)
};

/// Mutable per-process state owned by the engine while a simulation runs.
struct Job {
    int pid = 0;
    Time arrival = 0;
    Time burst = 0;
    int priority = 0;

    Time remaining = 0;
    Time first_run = -1;
    Time completion = -1;
    Time ready_since = 0;          ///< when the job last entered the ready queue
    Time waited = 0;               ///< total time spent in the ready queue so far
    double dispatch_priority = 0;  ///< effective (aged) priority at the last dispatch
    int level = 0;                 ///< MLFQ queue level (0 = highest)
    Time used_at_level = 0;        ///< MLFQ: CPU time consumed at the current level
    std::uint64_t seq = 0;         ///< global enqueue counter, breaks ties in FIFO order

    explicit Job(const Process& p)
        : pid(p.pid), arrival(p.arrival), burst(p.burst), priority(p.priority), remaining(p.burst) {}
};

enum class SliceKind { Run, Idle, ContextSwitch };

/// A contiguous interval of the CPU timeline.
struct Slice {
    SliceKind kind = SliceKind::Run;
    int pid = -1;  ///< -1 for Idle / ContextSwitch
    Time start = 0;
    Time end = 0;
    bool operator==(const Slice&) const = default;
};

struct ProcessStats {
    int pid = 0;
    Time arrival = 0;
    Time burst = 0;
    int priority = 0;
    Time start = 0;  ///< first time on the CPU
    Time completion = 0;
    Time turnaround = 0;  ///< completion - arrival
    Time waiting = 0;     ///< turnaround - burst (ready-queue time, incl. switch overhead)
    Time response = 0;    ///< start - arrival
};

struct Summary {
    double avg_waiting = 0;
    double avg_turnaround = 0;
    double avg_response = 0;
    double avg_slowdown = 0;  ///< mean turnaround / burst
    Time max_waiting = 0;
    Time makespan = 0;  ///< completion of the last job
    Time busy = 0;      ///< time spent running jobs
    double cpu_utilization = 0;
    double throughput = 0;  ///< jobs per time unit over the makespan
    int dispatches = 0;
    int context_switches = 0;  ///< times the running process changed (switch overhead is shown as
                               ///< ContextSwitch slices; a switch can be aborted by a preempting arrival)
    double fairness = 0;       ///< Jain's index over burst / turnaround (1 = perfectly fair)
};

struct Result {
    std::string algorithm;
    std::vector<Slice> timeline;
    std::vector<ProcessStats> processes;  ///< sorted by PID
    Summary summary;
};

}  // namespace sched
