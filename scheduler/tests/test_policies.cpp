// Textbook examples with hand-checked answers, plus behaviour of each policy's special rules.
#include <gtest/gtest.h>

#include <stdexcept>

#include "helpers.hpp"

using sched::Process;
using testing_helpers::run;
using testing_helpers::runs;
using testing_helpers::stats_of;
using V = std::vector<std::vector<long long>>;

// Silberschatz, Operating System Concepts (10th ed.), Section 5.3.2: SJF, all arrive at 0.
TEST(Textbook, SjfAllArriveAtZero) {
    const std::vector<Process> w{{1, 0, 6, 0}, {2, 0, 8, 0}, {3, 0, 7, 0}, {4, 0, 3, 0}};
    const auto r = run(w, "sjf");
    EXPECT_EQ(runs(r), (V{{4, 0, 3}, {1, 3, 9}, {3, 9, 16}, {2, 16, 24}}));
    EXPECT_DOUBLE_EQ(r.summary.avg_waiting, 7.0);
}

// Silberschatz 5.3.2: shortest-remaining-time-first with staggered arrivals.
TEST(Textbook, SrtfPreemptsOnShorterArrival) {
    const std::vector<Process> w{{1, 0, 8, 0}, {2, 1, 4, 0}, {3, 2, 9, 0}, {4, 3, 5, 0}};
    const auto r = run(w, "srtf");
    EXPECT_EQ(runs(r), (V{{1, 0, 1}, {2, 1, 5}, {4, 5, 10}, {1, 10, 17}, {3, 17, 26}}));
    EXPECT_DOUBLE_EQ(r.summary.avg_waiting, 6.5);
}

// Silberschatz 5.3.4: non-preemptive priority, lower number = higher priority.
TEST(Textbook, PriorityNonPreemptive) {
    const std::vector<Process> w{{1, 0, 10, 3}, {2, 0, 1, 1}, {3, 0, 2, 4}, {4, 0, 1, 5}, {5, 0, 5, 2}};
    const auto r = run(w, "prio");
    EXPECT_EQ(runs(r), (V{{2, 0, 1}, {5, 1, 6}, {1, 6, 16}, {3, 16, 18}, {4, 18, 19}}));
    EXPECT_DOUBLE_EQ(r.summary.avg_waiting, 8.2);
}

// Silberschatz 5.3.3: round robin with q = 4.
TEST(Textbook, RoundRobinQuantum4) {
    const std::vector<Process> w{{1, 0, 24, 0}, {2, 0, 3, 0}, {3, 0, 3, 0}};
    sched::PolicyConfig cfg;
    cfg.quantum = 4;
    const auto r = run(w, "rr", cfg);
    // Once P1 is alone it is still re-dispatched at every quantum boundary (no context switch, though).
    EXPECT_EQ(runs(r),
              (V{{1, 0, 4}, {2, 4, 7}, {3, 7, 10}, {1, 10, 14}, {1, 14, 18}, {1, 18, 22}, {1, 22, 26}, {1, 26, 30}}));
    EXPECT_EQ(r.summary.context_switches, 3);
    EXPECT_NEAR(r.summary.avg_waiting, 17.0 / 3.0, 1e-12);
}

// Stallings, Operating Systems (9th ed.), Table 9.5 – processes A..E as PIDs 1..5.
const std::vector<Process> kStallings{{1, 0, 3, 0}, {2, 2, 6, 0}, {3, 4, 4, 0}, {4, 6, 5, 0}, {5, 8, 2, 0}};

TEST(Textbook, StallingsFinishTimes) {
    auto finish = [](const sched::Result& r) {
        std::vector<long long> f;
        for (const auto& p : r.processes) f.push_back(p.completion);
        return f;
    };
    EXPECT_EQ(finish(run(kStallings, "fcfs")), (std::vector<long long>{3, 9, 13, 18, 20}));
    EXPECT_EQ(finish(run(kStallings, "sjf")), (std::vector<long long>{3, 9, 15, 20, 11}));
    EXPECT_EQ(finish(run(kStallings, "srtf")), (std::vector<long long>{3, 15, 8, 20, 10}));
    EXPECT_EQ(finish(run(kStallings, "hrrn")), (std::vector<long long>{3, 9, 13, 20, 15}));
}

TEST(Policies, RoundRobinRequeuesArrivalsBeforeExpiredJob) {
    // P2 arrives exactly when P1's slice expires: P2 must run before P1 continues.
    const std::vector<Process> w{{1, 0, 4, 0}, {2, 2, 2, 0}};
    const auto r = run(w, "rr");
    EXPECT_EQ(runs(r), (V{{1, 0, 2}, {2, 2, 4}, {1, 4, 6}}));
}

TEST(Policies, IdleGapsAreRecorded) {
    const std::vector<Process> w{{1, 3, 2, 0}, {2, 9, 1, 0}};
    const auto r = run(w, "fcfs");
    ASSERT_EQ(r.timeline.size(), 4u);
    EXPECT_EQ(r.timeline[0], (sched::Slice{sched::SliceKind::Idle, -1, 0, 3}));
    EXPECT_EQ(r.timeline[2], (sched::Slice{sched::SliceKind::Idle, -1, 5, 9}));
    EXPECT_DOUBLE_EQ(r.summary.cpu_utilization, 3.0 / 7.0);
}

TEST(Policies, ContextSwitchCostIsChargedAsWaiting) {
    const std::vector<Process> w{{1, 0, 3, 0}, {2, 0, 2, 0}};
    const auto r = run(w, "fcfs", {}, 1);
    ASSERT_EQ(r.timeline.size(), 3u);
    EXPECT_EQ(r.timeline[1], (sched::Slice{sched::SliceKind::ContextSwitch, -1, 3, 4}));
    EXPECT_EQ(stats_of(r, 2).waiting, 4);
    EXPECT_EQ(r.summary.context_switches, 1);
}

TEST(Policies, PreemptivePriorityAndTies) {
    const std::vector<Process> w{{1, 0, 5, 3}, {2, 2, 2, 1}, {3, 2, 2, 3}};
    const auto r = run(w, "prio-p");
    // P2 preempts P1 at t=2; P1 (arrived earlier) beats P3 on the priority tie.
    EXPECT_EQ(runs(r), (V{{1, 0, 2}, {2, 2, 4}, {1, 4, 7}, {3, 7, 9}}));
}

TEST(Policies, AgingPreventsStarvation) {
    std::vector<Process> w{{1, 0, 4, 9}};
    for (int i = 0; i < 40; ++i) w.push_back({i + 2, 1 + 2 * i, 2, 1});  // endless stream of urgent jobs
    const auto starving = run(w, "prio");
    sched::PolicyConfig aged;
    aged.aging = 2;  // one priority level per 2 time units of waiting
    const auto fair = run(w, "prio", aged);
    EXPECT_EQ(stats_of(starving, 1).completion, 4);  // non-preemptive: P1 was already running
    const auto starving_p = run(w, "prio-p");
    const auto fair_p = run(w, "prio-p", aged);
    EXPECT_EQ(stats_of(starving_p, 1).completion, 84);  // only after the whole stream (t = 81) + 3
    EXPECT_EQ(stats_of(fair_p, 1).completion, 20);      // aged to priority 1 by t = 17
    EXPECT_EQ(fair.algorithm, "Priority(aging=2)");
}

TEST(Policies, MlfqDemotesCpuBoundJobsAndFavoursNewcomers) {
    const std::vector<Process> w{{1, 0, 20, 0}, {2, 5, 2, 0}};
    sched::PolicyConfig cfg;
    cfg.mlfq_quanta = {2, 4, 8};
    const auto r = run(w, "mlfq", cfg);
    // P1: 2 units at level 0; at level 1 it is preempted at t=5 by P2 (level 0) after 3 of its 4 units,
    // keeps that allotment (1 unit left), is demoted to level 2 (q=8) and finally round-robins there.
    EXPECT_EQ(runs(r), (V{{1, 0, 2}, {1, 2, 5}, {2, 5, 7}, {1, 7, 8}, {1, 8, 16}, {1, 16, 22}}));
    EXPECT_EQ(stats_of(r, 2).response, 0);
}

TEST(Policies, MlfqBoostLetsLongJobsProgress) {
    // A long job and a steady stream of short level-0 jobs; boosts bring the long job back to the top.
    std::vector<Process> w{{1, 0, 50, 0}};
    for (int i = 0; i < 60; ++i) w.push_back({i + 2, 2 + 2 * i, 2, 0});
    sched::PolicyConfig cfg;
    cfg.mlfq_quanta = {2, 4};
    const auto no_boost = run(w, "mlfq", cfg);
    cfg.mlfq_boost = 20;
    const auto boost = run(w, "mlfq", cfg);
    auto second_run = [](const sched::Result& r) {
        for (const auto& s : runs(r)) {
            if (s[0] == 1 && s[1] > 0) return s[1];
        }
        return -1LL;
    };
    // Total work is fixed, so P1 finishes last either way; what changes is that it is no longer starved.
    EXPECT_EQ(second_run(no_boost), 122);  // only after all 60 short jobs
    EXPECT_EQ(second_run(boost), 22);      // right after the first boost at t = 20
    EXPECT_NE(boost.algorithm.find("boost=20"), std::string::npos);
}

TEST(Policies, DegenerateConfigurationsMatchFcfs) {
    const std::vector<Process> w{{1, 0, 5, 2}, {2, 1, 3, 1}, {3, 2, 8, 3}, {4, 7, 2, 1}};
    const auto fcfs = run(w, "fcfs");
    sched::PolicyConfig big;
    big.quantum = 1000;
    big.mlfq_quanta = {1000};
    EXPECT_EQ(runs(run(w, "rr", big)), runs(fcfs));
    EXPECT_EQ(runs(run(w, "mlfq", big)), runs(fcfs));
}

TEST(Policies, InvalidConfigurationIsRejected) {
    sched::PolicyConfig cfg;
    cfg.quantum = 0;  // the original program looped forever here
    EXPECT_THROW((void)sched::make_policy("rr", cfg), std::invalid_argument);
    cfg.mlfq_quanta = {};
    EXPECT_THROW((void)sched::make_policy("mlfq", cfg), std::invalid_argument);
    cfg.aging = -1;
    EXPECT_THROW((void)sched::make_policy("prio", cfg), std::invalid_argument);
    EXPECT_THROW((void)sched::make_policy("lottery"), std::invalid_argument);
    EXPECT_NO_THROW((void)sched::make_policy("SRTF"));
}

TEST(Engine, RejectsInvalidWorkloads) {
    auto policy = sched::make_policy("fcfs");
    EXPECT_THROW((void)sched::simulate({}, *policy), std::invalid_argument);
    EXPECT_THROW((void)sched::simulate({{1, 0, 0, 0}}, *policy), std::invalid_argument);
    EXPECT_THROW((void)sched::simulate({{1, -1, 3, 0}}, *policy), std::invalid_argument);
    EXPECT_THROW((void)sched::simulate({{1, 0, 3, 0}, {1, 2, 3, 0}}, *policy), std::invalid_argument);
    EXPECT_THROW((void)sched::simulate({{1, 0, 3, 0}}, *policy, sched::EngineOptions{-1}), std::invalid_argument);
}
