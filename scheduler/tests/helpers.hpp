#pragma once

#include <string>
#include <vector>

#include "sched/engine.hpp"
#include "sched/policy.hpp"

namespace testing_helpers {

inline sched::Result run(const std::vector<sched::Process>& w, const std::string& policy,
                         const sched::PolicyConfig& cfg = {}, sched::Time cs = 0) {
    auto p = sched::make_policy(policy, cfg);
    return sched::simulate(w, *p, sched::EngineOptions{cs});
}

inline const sched::ProcessStats& stats_of(const sched::Result& r, int pid) {
    for (const auto& p : r.processes) {
        if (p.pid == pid) return p;
    }
    throw std::out_of_range("pid");
}

/// Run slices only, as (pid, start, end) – easy to compare against textbook Gantt charts.
inline std::vector<std::vector<long long>> runs(const sched::Result& r) {
    std::vector<std::vector<long long>> out;
    for (const auto& s : r.timeline) {
        if (s.kind == sched::SliceKind::Run) out.push_back({s.pid, s.start, s.end});
    }
    return out;
}

}  // namespace testing_helpers
