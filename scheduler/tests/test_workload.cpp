#include <gtest/gtest.h>

#include <numeric>

#include "sched/workload.hpp"

TEST(Prng, SplitMix64ReferenceValues) {
    // Reference sequence of SplitMix64 for seed 0 (Vigna); guarantees cross-platform workloads.
    sched::SplitMix64 rng(0);
    EXPECT_EQ(rng.next(), 0xE220A8397B1DCDAFULL);
    EXPECT_EQ(rng.next(), 0x6E789E6AA1B965F4ULL);
    EXPECT_EQ(rng.next(), 0x06C45D188009454FULL);
    for (int i = 0; i < 1000; ++i) {
        const double u = rng.uniform();
        ASSERT_GE(u, 0.0);
        ASSERT_LT(u, 1.0);
    }
}

TEST(Workload, ReproducibleAndWellFormed) {
    sched::WorkloadSpec spec;
    spec.count = 500;
    spec.seed = 9;
    const auto a = sched::generate_workload(spec);
    const auto b = sched::generate_workload(spec);
    ASSERT_EQ(a.size(), 500u);
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].arrival, b[i].arrival);
        EXPECT_EQ(a[i].burst, b[i].burst);
        EXPECT_GE(a[i].burst, 1);
        EXPECT_GE(a[i].priority, 1);
        EXPECT_LE(a[i].priority, spec.priority_levels);
        if (i) EXPECT_LE(a[i - 1].arrival, a[i].arrival);
    }
}

class Distributions : public ::testing::TestWithParam<sched::BurstDistribution> {};

TEST_P(Distributions, MeanBurstAndLoadMatchTheSpec) {
    sched::WorkloadSpec spec;
    spec.count = 40000;
    spec.mean_burst = 20;
    spec.load = 0.8;
    spec.bursts = GetParam();
    const auto w = sched::generate_workload(spec);
    const double mean =
        std::accumulate(w.begin(), w.end(), 0.0,
                        [](double s, const sched::Process& p) { return s + static_cast<double>(p.burst); }) /
        static_cast<double>(w.size());
    EXPECT_NEAR(mean, 20.0, 1.0);
    const double offered = static_cast<double>(w.size()) * mean / static_cast<double>(w.back().arrival);
    EXPECT_NEAR(offered, 0.8, 0.04);
}

INSTANTIATE_TEST_SUITE_P(All, Distributions,
                         ::testing::Values(sched::BurstDistribution::Exponential, sched::BurstDistribution::Uniform,
                                           sched::BurstDistribution::Bimodal));

TEST(Workload, InvalidSpecsThrow) {
    sched::WorkloadSpec spec;
    spec.count = 0;
    EXPECT_THROW((void)sched::generate_workload(spec), std::invalid_argument);
    EXPECT_THROW((void)sched::parse_distribution("pareto"), std::invalid_argument);
    EXPECT_EQ(sched::parse_distribution("EXP"), sched::BurstDistribution::Exponential);
}
