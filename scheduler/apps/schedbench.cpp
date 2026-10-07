// schedbench – compare policies on many random workloads, and measure engine scaling.
//
//   schedbench                                    # default study, markdown table
//   schedbench --loads 0.5,0.8,0.95 --dist bimodal --reps 30 --format csv
//   schedbench --scaling                          # simulation time vs. number of processes
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "sched/engine.hpp"
#include "sched/policy.hpp"
#include "sched/workload.hpp"

namespace {

struct Config {
    int n = 400;
    int reps = 20;
    std::vector<double> loads{0.5, 0.8, 0.95};
    sched::BurstDistribution dist = sched::BurstDistribution::Exponential;
    double mean = 10;
    std::vector<std::string> algos = sched::policy_names();
    sched::PolicyConfig policy{4, {4, 8, 16}, 400, 50};
    bool scaling = false;
    bool csv = false;
};

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream in(s);
    for (std::string item; std::getline(in, item, ',');) out.push_back(item);
    return out;
}

double percentile(std::vector<double> v, double q) {
    std::sort(v.begin(), v.end());
    const double pos = q * static_cast<double>(v.size() - 1);
    const auto lo = static_cast<std::size_t>(std::floor(pos));
    const auto hi = std::min(lo + 1, v.size() - 1);
    return v[lo] + (v[hi] - v[lo]) * (pos - static_cast<double>(lo));
}

struct Acc {
    std::vector<double> wt, rt, slow, p95, fair, micros;
    void add(double w, double r, double s, double p, double f, double us) {
        wt.push_back(w), rt.push_back(r), slow.push_back(s), p95.push_back(p), fair.push_back(f), micros.push_back(us);
    }
};

double mean(const std::vector<double>& v) {
    return std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
}
double sd(const std::vector<double>& v) {
    const double m = mean(v);
    double s = 0;
    for (double x : v) s += (x - m) * (x - m);
    return v.size() > 1 ? std::sqrt(s / static_cast<double>(v.size() - 1)) : 0;
}

int study(const Config& c) {
    if (c.csv) {
        std::cout << "load,policy,avg_waiting,avg_waiting_sd,avg_response,avg_slowdown,p95_slowdown,fairness,sim_us\n";
    } else {
        std::cout << "Workloads: n=" << c.n << ", " << c.reps << " seeds per load, mean burst " << c.mean
                  << ". Values are means over seeds (sd for waiting).\n\n";
    }
    for (double load : c.loads) {
        std::map<std::string, Acc> acc;
        std::vector<std::string> order;
        for (int rep = 0; rep < c.reps; ++rep) {
            sched::WorkloadSpec spec;
            spec.count = c.n;
            spec.load = load;
            spec.mean_burst = c.mean;
            spec.bursts = c.dist;
            spec.seed = std::uint64_t{1000003} * static_cast<std::uint64_t>(rep) + 1;
            const auto work = sched::generate_workload(spec);
            for (const auto& name : c.algos) {
                auto policy = sched::make_policy(name, c.policy);
                const auto t0 = std::chrono::steady_clock::now();
                const auto r = sched::simulate(work, *policy);
                const double us =
                    std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
                std::vector<double> slow;
                for (const auto& p : r.processes) {
                    slow.push_back(static_cast<double>(p.turnaround) / static_cast<double>(p.burst));
                }
                if (!acc.count(r.algorithm)) order.push_back(r.algorithm);
                acc[r.algorithm].add(r.summary.avg_waiting, r.summary.avg_response, r.summary.avg_slowdown,
                                     percentile(slow, 0.95), r.summary.fairness, us);
            }
        }
        if (!c.csv) {
            std::cout << "### load " << load << "\n\n| policy | avg waiting | avg response | avg slowdown | "
                      << "p95 slowdown | Jain fairness | sim time (us) |\n|---|---|---|---|---|---|---|\n";
        }
        std::cout << std::fixed << std::setprecision(2);
        for (const auto& name : order) {
            const auto& a = acc[name];
            if (c.csv) {
                std::cout << load << ",\"" << name << "\"," << mean(a.wt) << ',' << sd(a.wt) << ',' << mean(a.rt) << ','
                          << mean(a.slow) << ',' << mean(a.p95) << ',' << std::setprecision(4) << mean(a.fair)
                          << std::setprecision(2) << ',' << mean(a.micros) << '\n';
            } else {
                std::cout << "| " << name << " | " << mean(a.wt) << " ± " << sd(a.wt) << " | " << mean(a.rt) << " | "
                          << mean(a.slow) << " | " << mean(a.p95) << " | " << std::setprecision(3) << mean(a.fair)
                          << std::setprecision(2) << " | " << mean(a.micros) << " |\n";
            }
        }
        std::cout.unsetf(std::ios::floatfield);
        if (!c.csv) std::cout << "\n";
    }
    return 0;
}

int scaling(const Config& c) {
    std::cout << "| processes | policy | sim time (ms) | ns per process |\n|---|---|---|---|\n";
    for (int n : {1000, 10000, 100000, 1000000}) {
        sched::WorkloadSpec spec;
        spec.count = n;
        spec.load = 0.9;
        spec.mean_burst = c.mean;
        const auto work = sched::generate_workload(spec);
        for (const std::string name : {"fcfs", "srtf", "prio", "rr", "mlfq"}) {
            auto policy = sched::make_policy(name, c.policy);
            const auto t0 = std::chrono::steady_clock::now();
            const auto r = sched::simulate(work, *policy);
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            std::cout << "| " << n << " | " << r.algorithm << " | " << std::fixed << std::setprecision(1) << ms << " | "
                      << std::setprecision(0) << ms * 1e6 / n << " |\n";
            std::cout.unsetf(std::ios::floatfield);
        }
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        Config c;
        for (int i = 1; i < argc; ++i) {
            const std::string a = argv[i];
            auto value = [&]() -> std::string {
                if (i + 1 >= argc) throw std::invalid_argument(a + " needs a value");
                return argv[++i];
            };
            if (a == "--n")
                c.n = std::stoi(value());
            else if (a == "--reps")
                c.reps = std::stoi(value());
            else if (a == "--loads") {
                c.loads.clear();
                for (const auto& s : split(value())) c.loads.push_back(std::stod(s));
            } else if (a == "--dist")
                c.dist = sched::parse_distribution(value());
            else if (a == "--mean")
                c.mean = std::stod(value());
            else if (a == "--algo")
                c.algos = split(value());
            else if (a == "-q" || a == "--quantum")
                c.policy.quantum = std::stoll(value());
            else if (a == "--scaling")
                c.scaling = true;
            else if (a == "--format")
                c.csv = value() == "csv";
            else if (a == "-h" || a == "--help") {
                std::cout << "usage: schedbench [--n N] [--reps R] [--loads a,b] [--dist exp|uniform|bimodal] "
                             "[--mean M] [--algo list] [-q Q] [--format md|csv] [--scaling]\n";
                return 0;
            } else
                throw std::invalid_argument("unknown option '" + a + "'");
        }
        return c.scaling ? scaling(c) : study(c);
    } catch (const std::exception& e) {
        std::cerr << "schedbench: error: " << e.what() << "\n";
        return 2;
    }
}
