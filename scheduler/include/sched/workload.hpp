// Reproducible synthetic workloads.
//
// Uses its own PRNG (SplitMix64) and inverse-transform sampling instead of
// <random> distributions, whose output is implementation-defined; the same seed
// therefore gives the same workload with libstdc++, libc++ and MSVC.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "sched/types.hpp"

namespace sched {

class SplitMix64 {
public:
    explicit SplitMix64(std::uint64_t seed) : state_(seed) {}
    std::uint64_t next();
    double uniform();  ///< [0, 1)

private:
    std::uint64_t state_;
};

enum class BurstDistribution { Exponential, Uniform, Bimodal };

struct WorkloadSpec {
    int count = 100;
    double load = 0.8;  ///< offered load rho = arrival rate * mean burst
    double mean_burst = 10;
    BurstDistribution bursts = BurstDistribution::Exponential;
    double bimodal_short_fraction = 0.9;  ///< share of short jobs (mean burst/4); long jobs keep the overall mean
    int priority_levels = 5;
    std::uint64_t seed = 1;
};

[[nodiscard]] BurstDistribution parse_distribution(const std::string& name);
[[nodiscard]] std::vector<Process> generate_workload(const WorkloadSpec& spec);

}  // namespace sched
