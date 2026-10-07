// Scheduling policies. A policy owns the ready queue; the engine owns time.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "sched/types.hpp"

namespace sched {

class Policy {
public:
    virtual ~Policy() = default;

    /// Human-readable name including parameters, e.g. "RR(q=2)".
    [[nodiscard]] virtual std::string name() const = 0;

    /// A job becomes ready at time `now` (arrival, preemption or quantum expiry).
    virtual void add(Job* job, Time now) = 0;

    /// Remove and return the next job to run, or nullptr if the ready queue is empty.
    virtual Job* pop(Time now) = 0;

    [[nodiscard]] virtual bool empty() const = 0;

    /// Maximum CPU time the job may use before it is put back (kNever = run to completion).
    [[nodiscard]] virtual Time time_slice(const Job& /*job*/) const { return kNever; }

    /// Whether arrivals are able to preempt the running job at all.
    [[nodiscard]] virtual bool preemptive() const { return false; }

    /// Should `arrived` (just added to the ready queue) preempt `running`?
    [[nodiscard]] virtual bool should_preempt(const Job& /*running*/, const Job& /*arrived*/, Time /*now*/) const {
        return false;
    }

    /// Called when the running job used up its whole time slice (before it is re-added).
    virtual void on_slice_expired(Job& /*job*/, Time /*now*/) {}

    /// Called when a job is dispatched.
    virtual void on_dispatch(Job& /*job*/, Time /*now*/) {}

    /// Called after `ran` time units of CPU (completion, expiry or preemption).
    virtual void on_ran(Job& /*job*/, Time /*ran*/) {}

    /// Time of the next policy-internal event (e.g. MLFQ priority boost), kNever if none.
    [[nodiscard]] virtual Time next_event(Time /*now*/) const { return kNever; }

    /// Handle the policy event at `now`. Returns true if the running job must be put back.
    virtual bool on_event(Time /*now*/, Job* /*running*/) { return false; }
};

/// Parameters for building policies from the command line / tests.
struct PolicyConfig {
    Time quantum = 2;                        ///< RR quantum
    std::vector<Time> mlfq_quanta{2, 4, 8};  ///< per-level quantum; last level is round robin
    Time mlfq_boost = 0;                     ///< priority-boost period (0 = off)
    double aging = 0;                        ///< priority aging: 1 level per `aging` time units waited (0 = off)
};

/// Names accepted by make_policy (case-insensitive).
[[nodiscard]] std::vector<std::string> policy_names();

/// Build a policy: fcfs, sjf, srtf, hrrn, prio, prio-p, rr, mlfq.
/// Throws std::invalid_argument on an unknown name or invalid parameters.
[[nodiscard]] std::unique_ptr<Policy> make_policy(const std::string& name, const PolicyConfig& cfg = {});

}  // namespace sched
