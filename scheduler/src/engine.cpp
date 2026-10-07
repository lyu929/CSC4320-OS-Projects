#include "sched/engine.hpp"

#include <algorithm>
#include <cassert>
#include <set>
#include <stdexcept>
#include <string>

namespace sched {

void validate(const std::vector<Process>& processes) {
    if (processes.empty()) throw std::invalid_argument("workload is empty");
    std::set<int> seen;
    for (const auto& p : processes) {
        const std::string who = "process " + std::to_string(p.pid);
        if (!seen.insert(p.pid).second) throw std::invalid_argument("duplicate PID " + std::to_string(p.pid));
        if (p.arrival < 0) throw std::invalid_argument(who + ": arrival time must be >= 0");
        if (p.burst <= 0) throw std::invalid_argument(who + ": burst time must be > 0");
    }
}

Summary summarize(const std::vector<ProcessStats>& stats, const std::vector<Slice>& timeline, int dispatches,
                  int context_switches) {
    Summary s;
    const auto n = static_cast<double>(stats.size());
    Time first_arrival = kNever;
    double sum_x = 0, sum_x2 = 0;
    for (const auto& p : stats) {
        s.avg_waiting += static_cast<double>(p.waiting) / n;
        s.avg_turnaround += static_cast<double>(p.turnaround) / n;
        s.avg_response += static_cast<double>(p.response) / n;
        s.avg_slowdown += static_cast<double>(p.turnaround) / static_cast<double>(p.burst) / n;
        s.max_waiting = std::max(s.max_waiting, p.waiting);
        s.makespan = std::max(s.makespan, p.completion);
        first_arrival = std::min(first_arrival, p.arrival);
        const double x = static_cast<double>(p.burst) / static_cast<double>(p.turnaround);
        sum_x += x;
        sum_x2 += x * x;
    }
    for (const auto& sl : timeline) {
        if (sl.kind == SliceKind::Run) s.busy += sl.end - sl.start;
    }
    const Time span = s.makespan - first_arrival;
    s.cpu_utilization = span > 0 ? static_cast<double>(s.busy) / static_cast<double>(span) : 1.0;
    s.throughput = span > 0 ? n / static_cast<double>(span) : 0.0;
    s.dispatches = dispatches;
    s.context_switches = context_switches;
    s.fairness = sum_x2 > 0 ? sum_x * sum_x / (n * sum_x2) : 1.0;
    return s;
}

Result simulate(const std::vector<Process>& processes, Policy& policy, const EngineOptions& options) {
    validate(processes);
    if (options.context_switch < 0) throw std::invalid_argument("context-switch cost must be >= 0");

    std::vector<Job> jobs;
    jobs.reserve(processes.size());
    for (const auto& p : processes) jobs.emplace_back(p);
    std::vector<Job*> order;
    order.reserve(jobs.size());
    for (auto& j : jobs) order.push_back(&j);
    std::stable_sort(order.begin(), order.end(), [](const Job* a, const Job* b) {
        return std::tie(a->arrival, a->pid) < std::tie(b->arrival, b->pid);
    });

    Result result;
    result.algorithm = policy.name();
    auto& timeline = result.timeline;

    const std::size_t n = jobs.size();
    std::size_t next = 0;  // next not-yet-arrived job in `order`
    std::size_t done = 0;
    Time now = 0;
    Job* running = nullptr;
    Job* expired = nullptr;  // slice ran out; re-queued after same-time arrivals
    Time slice_end = kNever;
    int last_pid = -1;
    int dispatches = 0;
    int switches = 0;
    bool fresh_dispatch = false;
    std::uint64_t seq = 0;
    std::vector<Job*> arrived;

    int last_run_pid = -1;
    auto push_slice = [&](SliceKind kind, int pid, Time start, Time end) {
        if (end <= start) return;
        if (kind == SliceKind::Run) {
            if (last_run_pid != -1 && pid != last_run_pid) ++switches;  // the running process changed
            last_run_pid = pid;
        }
        if (!fresh_dispatch && !timeline.empty() && timeline.back().kind == kind && timeline.back().pid == pid &&
            timeline.back().end == start) {
            timeline.back().end = end;  // same dispatch, split only by an evaluated event
        } else {
            timeline.push_back({kind, pid, start, end});
        }
        fresh_dispatch = false;
    };
    auto admit = [&] {
        arrived.clear();
        while (next < n && order[next]->arrival <= now) {
            Job* j = order[next++];
            j->ready_since = j->arrival;
            j->seq = seq++;
            policy.add(j, j->arrival);
            arrived.push_back(j);
        }
    };
    auto requeue = [&](Job* j) {
        j->ready_since = now;
        j->seq = seq++;
        policy.add(j, now);
    };

    while (done < n) {
        admit();
        if (expired != nullptr) {
            policy.on_slice_expired(*expired, now);
            requeue(expired);
            expired = nullptr;
        }
        while (policy.next_event(now) <= now) {
            if (policy.on_event(now, running) && running != nullptr) {
                requeue(running);
                running = nullptr;
            }
        }
        if (running != nullptr && policy.preemptive()) {
            for (const Job* a : arrived) {
                if (policy.should_preempt(*running, *a, now)) {
                    requeue(running);
                    running = nullptr;
                    break;
                }
            }
        }

        if (running == nullptr) {
            if (policy.empty()) {
                // Nothing ready: the CPU idles until the next arrival (one must exist).
                const Time t = order[next]->arrival;
                push_slice(SliceKind::Idle, -1, now, t);
                now = t;
                continue;
            }
            Job* j = policy.pop(now);
            ++dispatches;
            if (last_pid != -1 && j->pid != last_pid) {
                if (options.context_switch > 0) {
                    fresh_dispatch = true;
                    push_slice(SliceKind::ContextSwitch, -1, now, now + options.context_switch);
                    now += options.context_switch;
                }
            }
            j->waited += now - j->ready_since;
            if (j->first_run < 0) j->first_run = now;
            policy.on_dispatch(*j, now);
            const Time slice = policy.time_slice(*j);
            slice_end = slice >= kNever ? kNever : now + slice;
            running = j;
            last_pid = j->pid;
            fresh_dispatch = true;
        }

        Time t_next = std::min(now + running->remaining, slice_end);
        if (policy.preemptive() && next < n) t_next = std::min(t_next, order[next]->arrival);
        t_next = std::min(t_next, policy.next_event(now));
        if (t_next <= now) continue;  // an arrival during the context switch is due first

        const Time ran = t_next - now;
        push_slice(SliceKind::Run, running->pid, now, t_next);
        running->remaining -= ran;
        policy.on_ran(*running, ran);
        now = t_next;

        if (running->remaining == 0) {
            running->completion = now;
            assert(running->waited == running->completion - running->arrival - running->burst);
            ++done;
            running = nullptr;
            slice_end = kNever;
        } else if (now >= slice_end) {
            expired = running;
            running = nullptr;
            slice_end = kNever;
        }
    }

    result.processes.reserve(n);
    for (const auto& j : jobs) {
        ProcessStats s;
        s.pid = j.pid;
        s.arrival = j.arrival;
        s.burst = j.burst;
        s.priority = j.priority;
        s.start = j.first_run;
        s.completion = j.completion;
        s.turnaround = j.completion - j.arrival;
        s.waiting = s.turnaround - j.burst;
        s.response = j.first_run - j.arrival;
        result.processes.push_back(s);
    }
    std::sort(result.processes.begin(), result.processes.end(),
              [](const ProcessStats& a, const ProcessStats& b) { return a.pid < b.pid; });
    result.summary = summarize(result.processes, timeline, dispatches, switches);
    return result;
}

}  // namespace sched
