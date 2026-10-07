// schedsim – simulate CPU scheduling policies on a workload file.
//
//   schedsim processes.txt                       # FCFS + RR(q=2), text output
//   schedsim -a all --format compare processes.txt
//   schedsim -a srtf,rr -q 4 --cs 1 --svg out/chart processes.txt
//   schedsim --format legacy processes.txt 2     # byte-identical to the course program
//   schedsim gen -n 20 --seed 7 --load 0.9 > workload.txt
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "sched/engine.hpp"
#include "sched/io.hpp"
#include "sched/policy.hpp"
#include "sched/workload.hpp"

namespace {

constexpr const char* kVersion = "schedsim 1.0.0";

constexpr const char* kUsage = R"(usage: schedsim [options] [FILE [QUANTUM]]
       schedsim gen [-n N] [--seed S] [--load RHO] [--dist exp|uniform|bimodal] [--mean M]

Simulate CPU scheduling of the processes in FILE ("PID Arrival Burst Priority" per line;
default ./processes.txt). A second positional argument sets the RR quantum (legacy syntax).

options:
  -a, --algo LIST     comma-separated policies or 'all' (default fcfs,rr):
                      fcfs sjf srtf hrrn prio prio-p rr mlfq
  -q, --quantum N     round-robin quantum (default 2)
      --mlfq Q1/Q2/.. MLFQ per-level quanta, last level is round robin (default 2/4/8)
      --boost N       MLFQ priority-boost period, 0 = off (default 0)
      --aging X       priority aging: one level per X time units waited, 0 = off (default 0)
      --cs N          context-switch cost in time units (default 0)
  -f, --format F      text | compare | legacy | json | csv (default text)
      --svg PREFIX    also write PREFIX-<policy>.svg Gantt charts
  -o, --output FILE   write to FILE instead of stdout
  -h, --help          show this help
      --version       show the version
)";

struct Options {
    std::string file = "processes.txt";
    std::vector<std::string> algos{"fcfs", "rr"};
    sched::PolicyConfig policy;
    sched::EngineOptions engine;
    std::string format = "text";
    std::string svg_prefix;
    std::string output;
};

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::stringstream in(s);
    for (std::string item; std::getline(in, item, sep);) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

long long to_int(const std::string& flag, const std::string& v) {
    std::size_t pos = 0;
    long long x = 0;
    try {
        x = std::stoll(v, &pos);
    } catch (const std::exception&) {
        pos = 0;
    }
    if (pos != v.size()) throw std::invalid_argument(flag + " expects an integer, got '" + v + "'");
    return x;
}

double to_double(const std::string& flag, const std::string& v) {
    std::size_t pos = 0;
    double x = 0;
    try {
        x = std::stod(v, &pos);
    } catch (const std::exception&) {
        pos = 0;
    }
    if (pos != v.size()) throw std::invalid_argument(flag + " expects a number, got '" + v + "'");
    return x;
}

class Args {
public:
    Args(int argc, char** argv) : args_(argv + 1, argv + argc) {}
    bool done() const { return i_ >= args_.size(); }
    std::string next() { return args_[i_++]; }
    std::string value(const std::string& flag) {
        if (done()) throw std::invalid_argument(flag + " needs a value");
        return next();
    }

private:
    std::vector<std::string> args_;
    std::size_t i_ = 0;
};

int run_generate(Args& args) {
    sched::WorkloadSpec spec;
    while (!args.done()) {
        const std::string a = args.next();
        if (a == "-n")
            spec.count = static_cast<int>(to_int(a, args.value(a)));
        else if (a == "--seed")
            spec.seed = static_cast<std::uint64_t>(to_int(a, args.value(a)));
        else if (a == "--load")
            spec.load = to_double(a, args.value(a));
        else if (a == "--mean")
            spec.mean_burst = to_double(a, args.value(a));
        else if (a == "--dist")
            spec.bursts = sched::parse_distribution(args.value(a));
        else if (a == "--levels")
            spec.priority_levels = static_cast<int>(to_int(a, args.value(a)));
        else
            throw std::invalid_argument("unknown option '" + a + "' for gen");
    }
    std::cout << "# generated: n=" << spec.count << " seed=" << spec.seed << " load=" << spec.load
              << " mean=" << spec.mean_burst << "\n";
    sched::write_workload(std::cout, sched::generate_workload(spec));
    return 0;
}

Options parse(Args& args) {
    Options o;
    std::vector<std::string> positional;
    while (!args.done()) {
        const std::string a = args.next();
        if (a == "-h" || a == "--help") {
            std::cout << kUsage;
            std::exit(0);
        } else if (a == "--version") {
            std::cout << kVersion << "\n";
            std::exit(0);
        } else if (a == "-a" || a == "--algo") {
            const std::string v = args.value(a);
            o.algos = v == "all" ? sched::policy_names() : split(v, ',');
        } else if (a == "-q" || a == "--quantum") {
            o.policy.quantum = to_int(a, args.value(a));
        } else if (a == "--mlfq") {
            o.policy.mlfq_quanta.clear();
            for (const auto& q : split(args.value(a), '/')) o.policy.mlfq_quanta.push_back(to_int(a, q));
        } else if (a == "--boost") {
            o.policy.mlfq_boost = to_int(a, args.value(a));
        } else if (a == "--aging") {
            o.policy.aging = to_double(a, args.value(a));
        } else if (a == "--cs") {
            o.engine.context_switch = to_int(a, args.value(a));
        } else if (a == "-f" || a == "--format") {
            o.format = args.value(a);
        } else if (a == "--svg") {
            o.svg_prefix = args.value(a);
        } else if (a == "-o" || a == "--output") {
            o.output = args.value(a);
        } else if (!a.empty() && a[0] == '-') {
            throw std::invalid_argument("unknown option '" + a + "' (see --help)");
        } else {
            positional.push_back(a);
        }
    }
    if (positional.size() > 2) throw std::invalid_argument("too many positional arguments");
    if (!positional.empty()) o.file = positional[0];
    if (positional.size() == 2) o.policy.quantum = to_int("QUANTUM", positional[1]);
    const std::vector<std::string> formats{"text", "compare", "legacy", "json", "csv"};
    if (std::find(formats.begin(), formats.end(), o.format) == formats.end()) {
        throw std::invalid_argument("unknown format '" + o.format + "'");
    }
    return o;
}

// "MLFQ(q=2/4/8,boost=20)" -> "mlfq-q-2-4-8-boost-20"
std::string file_safe(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else if (!out.empty() && out.back() != '-') {
            out += '-';
        }
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        Args args(argc, argv);
        if (argc > 1 && std::string(argv[1]) == "gen") {
            args.next();
            return run_generate(args);
        }
        const Options o = parse(args);
        const auto workload = sched::load_workload(o.file);

        std::vector<sched::Result> results;
        for (const auto& name : o.algos) {
            auto policy = sched::make_policy(name, o.policy);
            results.push_back(sched::simulate(workload, *policy, o.engine));
        }

        std::ofstream file;
        if (!o.output.empty()) {
            file.open(o.output);
            if (!file) throw std::runtime_error("cannot write '" + o.output + "'");
        }
        std::ostream& out = o.output.empty() ? std::cout : file;

        if (o.format == "legacy") {
            out << "Operating System Scheduling Simulation\n"
                << "Input file: " << o.file << "\n";
            for (const auto& r : results) sched::render_legacy(out, r, r.algorithm);
        } else if (o.format == "json") {
            sched::render_json(out, results);
        } else if (o.format == "csv") {
            sched::render_csv(out, results);
        } else if (o.format == "compare") {
            sched::render_comparison(out, results);
        } else {
            for (const auto& r : results) sched::render_text(out, r);
            if (results.size() > 1) sched::render_comparison(out, results);
        }

        if (!o.svg_prefix.empty()) {
            for (const auto& r : results) {
                const std::string path = o.svg_prefix + "-" + file_safe(r.algorithm) + ".svg";
                std::ofstream svg(path);
                if (!svg) throw std::runtime_error("cannot write '" + path + "'");
                sched::render_svg(svg, r);
                std::cerr << "wrote " << path << "\n";
            }
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "schedsim: error: " << e.what() << "\n";
        return 2;
    }
}
