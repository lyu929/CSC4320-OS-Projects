#include "sched/workload.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace sched {

std::uint64_t SplitMix64::next() {
    std::uint64_t z = (state_ += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

double SplitMix64::uniform() {
    return static_cast<double>(next() >> 11) * 0x1.0p-53;
}

BurstDistribution parse_distribution(const std::string& name) {
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (n == "exp" || n == "exponential") return BurstDistribution::Exponential;
    if (n == "uniform") return BurstDistribution::Uniform;
    if (n == "bimodal") return BurstDistribution::Bimodal;
    throw std::invalid_argument("unknown burst distribution '" + name + "' (exp, uniform, bimodal)");
}

namespace {
double exponential(SplitMix64& rng, double mean) {
    return -mean * std::log1p(-rng.uniform());
}
}  // namespace

std::vector<Process> generate_workload(const WorkloadSpec& spec) {
    if (spec.count <= 0) throw std::invalid_argument("count must be positive");
    if (spec.load <= 0) throw std::invalid_argument("load must be positive");
    if (spec.mean_burst < 1) throw std::invalid_argument("mean burst must be >= 1");
    SplitMix64 rng(spec.seed);
    const double rate = spec.load / spec.mean_burst;  // Poisson arrivals at the requested utilisation
    // Bimodal: short jobs average mean/4; long jobs are sized so the overall mean is `mean`.
    const double p_short = spec.bimodal_short_fraction;
    const double short_mean = spec.mean_burst / 4;
    const double long_mean = (spec.mean_burst - p_short * short_mean) / (1 - p_short);
    std::vector<Process> out;
    out.reserve(static_cast<std::size_t>(spec.count));
    double clock = 0;
    for (int i = 0; i < spec.count; ++i) {
        if (i > 0) clock += exponential(rng, 1.0 / rate);
        double b = 0;
        switch (spec.bursts) {
            case BurstDistribution::Exponential: b = exponential(rng, spec.mean_burst); break;
            case BurstDistribution::Uniform: b = 1 + rng.uniform() * (2 * spec.mean_burst - 2); break;
            case BurstDistribution::Bimodal:
                b = rng.uniform() < p_short ? exponential(rng, short_mean) : exponential(rng, long_mean);
                break;
        }
        const int prio = 1 + static_cast<int>(rng.uniform() * spec.priority_levels);
        out.push_back({i + 1, static_cast<Time>(std::floor(clock)), std::max<Time>(1, std::llround(b)), prio});
    }
    return out;
}

}  // namespace sched
