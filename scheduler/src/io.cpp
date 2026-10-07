#include "sched/io.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

namespace sched {

ParseError::ParseError(const std::string& source, int line, const std::string& message)
    : std::runtime_error(source + ":" + std::to_string(line) + ": " + message), line_(line) {}

namespace {

std::vector<std::string> split_fields(const std::string& line) {
    std::string s = line;
    std::replace(s.begin(), s.end(), ',', ' ');
    std::istringstream in(s);
    std::vector<std::string> out;
    for (std::string tok; in >> tok;) out.push_back(tok);
    return out;
}

bool parse_int(const std::string& tok, long long& value) {
    const char* first = tok.data();
    const char* last = tok.data() + tok.size();
    auto [ptr, ec] = std::from_chars(first, last, value);
    return ec == std::errc() && ptr == last;
}

const char* kind_name(SliceKind k) {
    switch (k) {
        case SliceKind::Run: return "run";
        case SliceKind::Idle: return "idle";
        case SliceKind::ContextSwitch: return "switch";
    }
    return "?";
}

std::string slice_label(const Slice& s) {
    switch (s.kind) {
        case SliceKind::Run: return "P" + std::to_string(s.pid);
        case SliceKind::Idle: return "idle";
        case SliceKind::ContextSwitch: return "cs";
    }
    return "?";
}

std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        o += c;
    }
    return o;
}

std::string num(double v) {
    std::ostringstream s;
    s << std::setprecision(10) << v;
    return s.str();
}

}  // namespace

std::vector<Process> parse_workload(std::istream& in, const std::string& source) {
    std::vector<Process> out;
    std::map<int, int> first_line;  // pid -> line
    std::string line;
    int lineno = 0;
    bool seen_content = false;
    while (std::getline(in, line)) {
        ++lineno;
        if (auto hash = line.find('#'); hash != std::string::npos) line.erase(hash);
        auto fields = split_fields(line);
        if (fields.empty()) continue;
        long long probe = 0;
        if (!seen_content && !parse_int(fields[0], probe)) {  // header row
            seen_content = true;
            continue;
        }
        seen_content = true;
        if (fields.size() < 3 || fields.size() > 4) {
            throw ParseError(
                source, lineno,
                "expected 'PID Arrival Burst [Priority]', got " + std::to_string(fields.size()) + " fields");
        }
        long long v[4] = {0, 0, 0, 0};
        static const char* names[] = {"PID", "arrival", "burst", "priority"};
        for (std::size_t i = 0; i < fields.size(); ++i) {
            if (!parse_int(fields[i], v[i])) {
                throw ParseError(source, lineno, std::string(names[i]) + " is not an integer: '" + fields[i] + "'");
            }
        }
        if (v[1] < 0) throw ParseError(source, lineno, "arrival time must be >= 0");
        if (v[2] <= 0) throw ParseError(source, lineno, "burst time must be > 0");
        const int pid = static_cast<int>(v[0]);
        if (auto [it, fresh] = first_line.emplace(pid, lineno); !fresh) {
            throw ParseError(
                source, lineno,
                "duplicate PID " + std::to_string(pid) + " (first on line " + std::to_string(it->second) + ")");
        }
        out.push_back({pid, v[1], v[2], static_cast<int>(v[3])});
    }
    if (out.empty()) throw ParseError(source, lineno, "no processes found");
    return out;
}

std::vector<Process> load_workload(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open '" + path + "'");
    return parse_workload(in, path);
}

void write_workload(std::ostream& out, const std::vector<Process>& processes) {
    out << "PID Arrival Burst Priority\n";
    for (const auto& p : processes) out << p.pid << ' ' << p.arrival << ' ' << p.burst << ' ' << p.priority << '\n';
}

void render_legacy(std::ostream& out, const Result& r, const std::string& title) {
    // Byte-for-byte the format of the original course program.
    std::vector<Slice> runs;
    std::copy_if(r.timeline.begin(), r.timeline.end(), std::back_inserter(runs),
                 [](const Slice& s) { return s.kind == SliceKind::Run; });
    const bool rr = r.algorithm.starts_with("RR(q=");
    const std::string gantt_title =
        rr ? "Round Robin (q=" + r.algorithm.substr(5, r.algorithm.size() - 6) + ")" : title;
    out << "\n============================\n" << gantt_title << " Gantt Chart:\n";
    for (const auto& s : runs) out << "| P" << s.pid << " ";
    out << "|\n";
    if (!runs.empty()) {
        out << runs.front().start;
        for (const auto& s : runs) out << "    " << s.end;
        out << "\n";
    }
    out << "\n" << (rr ? "Round Robin" : title) << " Results:\n";
    out << "PID\tAT\tBT\tWT\tTAT\tCT\n";
    double wt = 0, tat = 0;
    for (const auto& p : r.processes) {
        wt += static_cast<double>(p.waiting);
        tat += static_cast<double>(p.turnaround);
        out << p.pid << "\t" << p.arrival << "\t" << p.burst << "\t" << p.waiting << "\t" << p.turnaround << "\t"
            << p.completion << "\n";
    }
    const auto n = static_cast<double>(r.processes.size());
    out << "Average WT = " << (wt / n) << "\n";
    out << "Average TAT = " << (tat / n) << "\n";
}

void render_text(std::ostream& out, const Result& r) {
    out << "== " << r.algorithm << " ==\n";
    std::string bar, axis;
    for (const auto& s : r.timeline) {
        const std::string label = slice_label(s);
        std::string cell = "| " + label + " ";
        const std::string t = std::to_string(s.start);
        axis += t + std::string(cell.size() > t.size() ? cell.size() - t.size() : 1, ' ');
        bar += cell;
    }
    bar += "|";
    if (!r.timeline.empty()) axis += std::to_string(r.timeline.back().end);
    out << bar << "\n" << axis << "\n\n";
    out << std::left << std::setw(6) << "PID" << std::right << std::setw(5) << "AT" << std::setw(5) << "BT"
        << std::setw(5) << "PR" << std::setw(7) << "Start" << std::setw(7) << "CT" << std::setw(7) << "TAT"
        << std::setw(7) << "WT" << std::setw(7) << "RT" << "\n";
    for (const auto& p : r.processes) {
        out << std::left << std::setw(6) << ("P" + std::to_string(p.pid)) << std::right << std::setw(5) << p.arrival
            << std::setw(5) << p.burst << std::setw(5) << p.priority << std::setw(7) << p.start << std::setw(7)
            << p.completion << std::setw(7) << p.turnaround << std::setw(7) << p.waiting << std::setw(7) << p.response
            << "\n";
    }
    const auto& s = r.summary;
    out << std::fixed << std::setprecision(2);
    out << "avg waiting " << s.avg_waiting << " | avg turnaround " << s.avg_turnaround << " | avg response "
        << s.avg_response << " | avg slowdown " << s.avg_slowdown << "\n";
    out << "CPU utilisation " << 100 * s.cpu_utilization << "% | throughput " << s.throughput
        << "/unit | context switches " << s.context_switches << " | Jain fairness " << s.fairness << "\n\n";
    out.unsetf(std::ios::floatfield);
    out << std::setprecision(6);
}

void render_comparison(std::ostream& out, const std::vector<Result>& results) {
    std::size_t w = 9;
    for (const auto& r : results) w = std::max(w, r.algorithm.size() + 1);
    out << std::left << std::setw(static_cast<int>(w)) << "algorithm" << std::right << std::setw(9) << "avg WT"
        << std::setw(9) << "avg TAT" << std::setw(9) << "avg RT" << std::setw(10) << "slowdown" << std::setw(8)
        << "max WT" << std::setw(8) << "util%" << std::setw(6) << "CS" << std::setw(9) << "fairness" << "\n";
    out << std::fixed << std::setprecision(2);
    for (const auto& r : results) {
        const auto& s = r.summary;
        out << std::left << std::setw(static_cast<int>(w)) << r.algorithm << std::right << std::setw(9) << s.avg_waiting
            << std::setw(9) << s.avg_turnaround << std::setw(9) << s.avg_response << std::setw(10) << s.avg_slowdown
            << std::setw(8) << s.max_waiting << std::setw(8) << 100 * s.cpu_utilization << std::setw(6)
            << s.context_switches << std::setw(9) << std::setprecision(3) << s.fairness << std::setprecision(2) << "\n";
    }
    out.unsetf(std::ios::floatfield);
    out << std::setprecision(6);
}

void render_json(std::ostream& out, const std::vector<Result>& results) {
    out << "[\n";
    for (std::size_t k = 0; k < results.size(); ++k) {
        const auto& r = results[k];
        const auto& s = r.summary;
        out << "  {\n    \"algorithm\": \"" << json_escape(r.algorithm) << "\",\n    \"timeline\": [";
        for (std::size_t i = 0; i < r.timeline.size(); ++i) {
            const auto& t = r.timeline[i];
            out << (i ? ", " : "") << "{\"kind\": \"" << kind_name(t.kind) << "\", \"pid\": " << t.pid
                << ", \"start\": " << t.start << ", \"end\": " << t.end << "}";
        }
        out << "],\n    \"processes\": [";
        for (std::size_t i = 0; i < r.processes.size(); ++i) {
            const auto& p = r.processes[i];
            out << (i ? ", " : "") << "{\"pid\": " << p.pid << ", \"arrival\": " << p.arrival
                << ", \"burst\": " << p.burst << ", \"priority\": " << p.priority << ", \"start\": " << p.start
                << ", \"completion\": " << p.completion << ", \"turnaround\": " << p.turnaround
                << ", \"waiting\": " << p.waiting << ", \"response\": " << p.response << "}";
        }
        out << "],\n    \"summary\": {\"avg_waiting\": " << num(s.avg_waiting)
            << ", \"avg_turnaround\": " << num(s.avg_turnaround) << ", \"avg_response\": " << num(s.avg_response)
            << ", \"avg_slowdown\": " << num(s.avg_slowdown) << ", \"max_waiting\": " << s.max_waiting
            << ", \"makespan\": " << s.makespan << ", \"busy\": " << s.busy
            << ", \"cpu_utilization\": " << num(s.cpu_utilization) << ", \"throughput\": " << num(s.throughput)
            << ", \"dispatches\": " << s.dispatches << ", \"context_switches\": " << s.context_switches
            << ", \"fairness\": " << num(s.fairness) << "}\n  }" << (k + 1 < results.size() ? "," : "") << "\n";
    }
    out << "]\n";
}

void render_csv(std::ostream& out, const std::vector<Result>& results) {
    out << "algorithm,pid,arrival,burst,priority,start,completion,turnaround,waiting,response\n";
    for (const auto& r : results) {
        for (const auto& p : r.processes) {
            out << '"' << r.algorithm << "\"," << p.pid << ',' << p.arrival << ',' << p.burst << ',' << p.priority
                << ',' << p.start << ',' << p.completion << ',' << p.turnaround << ',' << p.waiting << ',' << p.response
                << '\n';
        }
    }
}

void render_svg(std::ostream& out, const Result& r) {
    const double left = 70, top = 40, row = 24, width = 900;
    const Time t0 = r.timeline.empty() ? 0 : r.timeline.front().start;
    const Time t1 = r.timeline.empty() ? 1 : r.timeline.back().end;
    const double scale = (width - left - 20) / static_cast<double>(std::max<Time>(1, t1 - t0));
    std::vector<int> pids;
    pids.reserve(r.processes.size());
    for (const auto& p : r.processes) pids.push_back(p.pid);
    std::map<int, std::size_t> row_of;
    for (std::size_t i = 0; i < pids.size(); ++i) row_of[pids[i]] = i + 1;  // row 0 = CPU
    const double height = top + row * static_cast<double>(pids.size() + 1) + 40;
    auto x = [&](Time t) { return left + static_cast<double>(t - t0) * scale; };
    auto color = [&](int pid) {
        const auto i = static_cast<double>(row_of[pid]);
        std::ostringstream c;
        c << "hsl(" << std::fmod(i * 137.508, 360.0) << ",65%,55%)";
        return c.str();
    };
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width << "\" height=\"" << height
        << "\" font-family=\"sans-serif\" font-size=\"11\">\n";
    out << "<text x=\"" << left << "\" y=\"20\" font-size=\"14\" font-weight=\"bold\">" << json_escape(r.algorithm)
        << "  (avg WT " << num(r.summary.avg_waiting) << ", avg TAT " << num(r.summary.avg_turnaround) << ")</text>\n";
    out << "<text x=\"8\" y=\"" << top + row * 0.65 << "\" font-weight=\"bold\">CPU</text>\n";
    for (std::size_t i = 0; i < pids.size(); ++i) {
        out << "<text x=\"8\" y=\"" << top + row * (static_cast<double>(i) + 1.65) << "\">P" << pids[i] << "</text>\n";
    }
    for (const auto& p : r.processes) {  // waiting band from arrival to completion
        const double y = top + row * static_cast<double>(row_of[p.pid]) + 9;
        out << "<rect x=\"" << x(p.arrival) << "\" y=\"" << y << "\" width=\"" << x(p.completion) - x(p.arrival)
            << "\" height=\"4\" fill=\"#ddd\"/>\n";
    }
    for (const auto& s : r.timeline) {
        const double w = x(s.end) - x(s.start);
        if (s.kind == SliceKind::Run) {
            const std::string c = color(s.pid);
            out << "<rect x=\"" << x(s.start) << "\" y=\"" << top + 2 << "\" width=\"" << w << "\" height=\"" << row - 4
                << "\" fill=\"" << c << "\"><title>P" << s.pid << " " << s.start << "-" << s.end << "</title></rect>\n";
            out << "<rect x=\"" << x(s.start) << "\" y=\"" << top + row * static_cast<double>(row_of[s.pid]) + 2
                << "\" width=\"" << w << "\" height=\"" << row - 4 << "\" fill=\"" << c << "\"/>\n";
        } else {
            out << "<rect x=\"" << x(s.start) << "\" y=\"" << top + 2 << "\" width=\"" << w << "\" height=\"" << row - 4
                << "\" fill=\"" << (s.kind == SliceKind::Idle ? "#f0f0f0" : "#555") << "\"><title>" << slice_label(s)
                << " " << s.start << "-" << s.end << "</title></rect>\n";
        }
    }
    const double axis_y = top + row * static_cast<double>(pids.size() + 1) + 6;
    out << "<line x1=\"" << left << "\" y1=\"" << axis_y << "\" x2=\"" << x(t1) << "\" y2=\"" << axis_y
        << "\" stroke=\"#333\"/>\n";
    const Time span = std::max<Time>(1, t1 - t0);
    Time step = 1;  // 1, 2, 5, 10, 20, 50, ... so that there are at most ~20 ticks
    for (Time mag = 1; span / step > 20; mag *= 10) {
        for (Time m : {1, 2, 5}) {
            step = m * mag;
            if (span / step <= 20) break;
        }
    }
    std::set<Time> ticks;
    for (Time t = t0; t <= t1; t += step) ticks.insert(t);
    if (t1 - *ticks.rbegin() > step / 2) ticks.insert(t1);  // label the end unless it would overlap
    for (Time t : ticks) {
        out << "<line x1=\"" << x(t) << "\" y1=\"" << axis_y << "\" x2=\"" << x(t) << "\" y2=\"" << axis_y + 4
            << "\" stroke=\"#333\"/><text x=\"" << x(t) << "\" y=\"" << axis_y + 16 << "\" text-anchor=\"middle\">" << t
            << "</text>\n";
    }
    out << "</svg>\n";
}

}  // namespace sched
