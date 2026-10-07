// Workload parsing and result rendering (text, legacy text, JSON, CSV, SVG).
#pragma once

#include <iosfwd>
#include <stdexcept>
#include <string>
#include <vector>

#include "sched/types.hpp"

namespace sched {

class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& source, int line, const std::string& message);
    [[nodiscard]] int line() const noexcept { return line_; }

private:
    int line_;
};

/// Parse "PID Arrival Burst Priority" rows (whitespace or comma separated).
/// A header line, blank lines and '#' comments are ignored. Throws ParseError.
[[nodiscard]] std::vector<Process> parse_workload(std::istream& in, const std::string& source = "<input>");
[[nodiscard]] std::vector<Process> load_workload(const std::string& path);
void write_workload(std::ostream& out, const std::vector<Process>& processes);

/// Exactly the output format of the original course program (one algorithm).
void render_legacy(std::ostream& out, const Result& result, const std::string& title);

/// Gantt chart + per-process table + summary.
void render_text(std::ostream& out, const Result& result);

/// One row per algorithm, for --compare.
void render_comparison(std::ostream& out, const std::vector<Result>& results);

void render_json(std::ostream& out, const std::vector<Result>& results);
void render_csv(std::ostream& out, const std::vector<Result>& results);

/// Standalone SVG Gantt chart (one row per process plus a CPU row).
void render_svg(std::ostream& out, const Result& result);

}  // namespace sched
