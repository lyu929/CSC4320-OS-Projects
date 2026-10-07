// Invariants that must hold for every policy on every workload (randomised, seeded).
#include <gtest/gtest.h>

#include <map>
#include <numeric>

#include "helpers.hpp"
#include "sched/workload.hpp"

namespace {

using sched::SliceKind;

struct Case {
    std::string policy;
    sched::Time cs;
};

std::vector<sched::Process> workload(std::uint64_t seed, int n = 40) {
    sched::WorkloadSpec spec;
    spec.count = n;
    spec.seed = seed;
    spec.load = 0.6 + 0.1 * static_cast<double>(seed % 5);  // includes overloaded cases (rho >= 1)
    spec.mean_burst = 6;
    spec.bursts = static_cast<sched::BurstDistribution>(seed % 3);
    return sched::generate_workload(spec);
}

sched::PolicyConfig config() {
    sched::PolicyConfig c;
    c.quantum = 3;
    c.mlfq_quanta = {2, 4, 8};
    c.mlfq_boost = 30;
    c.aging = 5;
    return c;
}

void check_invariants(const std::vector<sched::Process>& w, const sched::Result& r, sched::Time cs) {
    std::map<int, sched::Process> proc;
    for (const auto& p : w) proc[p.pid] = p;
    std::map<int, sched::Time> served, last_end;

    // 1. the timeline is contiguous from t=0 and every slice is non-empty
    sched::Time t = 0;
    for (const auto& s : r.timeline) {
        ASSERT_EQ(s.start, t) << r.algorithm;
        ASSERT_LT(s.start, s.end) << r.algorithm;
        t = s.end;
        if (s.kind == SliceKind::Run) {
            ASSERT_GE(s.start, proc[s.pid].arrival) << "ran before arrival: " << r.algorithm;
            served[s.pid] += s.end - s.start;
            last_end[s.pid] = s.end;
        } else if (s.kind == SliceKind::ContextSwitch) {
            ASSERT_EQ(s.end - s.start, cs);
        } else {
            // 2. work conservation: idle only while every arrived job has finished
            for (const auto& p : w) {
                if (p.arrival <= s.start) {
                    ASSERT_EQ(served[p.pid], p.burst) << "idle while P" << p.pid << " was ready: " << r.algorithm;
                }
            }
        }
    }
    ASSERT_EQ(r.processes.size(), w.size());
    sched::Time busy = 0;
    for (const auto& s : r.processes) {
        const auto& p = proc[s.pid];
        // 3. every job gets exactly its burst and finishes when its last slice ends
        EXPECT_EQ(served[s.pid], p.burst) << r.algorithm;
        EXPECT_EQ(s.completion, last_end[s.pid]) << r.algorithm;
        // 4. metric identities
        EXPECT_EQ(s.turnaround, s.completion - p.arrival);
        EXPECT_EQ(s.waiting, s.turnaround - p.burst);
        EXPECT_GE(s.waiting, 0);
        EXPECT_GE(s.response, 0);
        EXPECT_LE(s.response, s.waiting);
        busy += p.burst;
    }
    EXPECT_EQ(r.summary.busy, busy);
    EXPECT_GT(r.summary.fairness, 0.0);
    EXPECT_LE(r.summary.fairness, 1.0 + 1e-12);
    // 5. context switches = changes of running process along the timeline
    int switches = 0, prev = -1;
    for (const auto& s : r.timeline) {
        if (s.kind != SliceKind::Run) continue;
        if (prev != -1 && s.pid != prev) ++switches;
        prev = s.pid;
    }
    EXPECT_EQ(r.summary.context_switches, switches) << r.algorithm;
}

class Invariants : public ::testing::TestWithParam<Case> {};

TEST_P(Invariants, HoldOnRandomWorkloads) {
    const auto [policy, cs] = GetParam();
    for (std::uint64_t seed = 1; seed <= 60; ++seed) {
        const auto w = workload(seed);
        const auto r = testing_helpers::run(w, policy, config(), cs);
        check_invariants(w, r, cs);
        if (HasFatalFailure()) {
            ADD_FAILURE() << "seed " << seed;
            return;
        }
    }
}

std::vector<Case> all_cases() {
    std::vector<Case> out;
    for (const auto& p : sched::policy_names()) {
        out.push_back({p, 0});
        out.push_back({p, 1});
    }
    return out;
}

INSTANTIATE_TEST_SUITE_P(AllPolicies, Invariants, ::testing::ValuesIn(all_cases()), [](const auto& info) {
    std::string n = info.param.policy + (info.param.cs ? "_cs" : "");
    for (char& c : n) {
        if (c == '-') c = '_';
    }
    return n;
});

TEST(Optimality, SrtfMinimisesMeanTurnaround) {
    // SRPT is optimal for mean flow time on one machine with release dates.
    for (std::uint64_t seed = 1; seed <= 100; ++seed) {
        const auto w = workload(seed, 30);
        const double best = testing_helpers::run(w, "srtf").summary.avg_turnaround;
        for (const auto& p : sched::policy_names()) {
            EXPECT_LE(best, testing_helpers::run(w, p, config()).summary.avg_turnaround + 1e-9)
                << p << " seed " << seed;
        }
    }
}

TEST(Determinism, SameInputSameOutput) {
    const auto w = workload(42, 200);
    for (const auto& p : sched::policy_names()) {
        const auto a = testing_helpers::run(w, p, config(), 1);
        const auto b = testing_helpers::run(w, p, config(), 1);
        EXPECT_EQ(a.timeline, b.timeline) << p;
    }
}

}  // namespace
