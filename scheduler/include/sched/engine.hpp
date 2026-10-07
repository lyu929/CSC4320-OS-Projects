// Event-driven single-CPU simulation engine.
#pragma once

#include <vector>

#include "sched/policy.hpp"
#include "sched/types.hpp"

namespace sched {

struct EngineOptions {
    Time context_switch = 0;  ///< overhead charged when the CPU switches to a different job
};

/// Simulate `processes` under `policy`.
///
/// Time advances from event to event (arrival, completion, slice expiry, policy event),
/// so the cost is O(events * queue operation) and independent of the burst lengths.
/// Arrivals that coincide with a slice expiry are enqueued *before* the expired job
/// (the convention used by most textbooks and by the original course implementation).
///
/// Throws std::invalid_argument if the workload is invalid (see validate()).
[[nodiscard]] Result simulate(const std::vector<Process>& processes, Policy& policy, const EngineOptions& options = {});

/// Throws std::invalid_argument for: empty workload, duplicate PIDs, negative arrival,
/// non-positive burst.
void validate(const std::vector<Process>& processes);

/// Compute per-process statistics and the summary from a finished timeline.
[[nodiscard]] Summary summarize(const std::vector<ProcessStats>& stats, const std::vector<Slice>& timeline,
                                int dispatches, int context_switches);

}  // namespace sched
