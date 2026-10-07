#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <sstream>

#include "helpers.hpp"
#include "sched/io.hpp"

namespace {

std::string slurp(const std::string& path) {
    std::ifstream in(path);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

TEST(Golden, LegacyOutputIsByteIdentical) {
    // The original course program's output for FCFS and RR(q=2) is reproduced exactly.
    const auto w = sched::load_workload(std::string(SCHED_LEGACY_DIR) + "/processes.txt");
    std::ostringstream out;
    out << "Operating System Scheduling Simulation\nInput file: processes.txt\n";
    for (const auto* name : {"fcfs", "rr"}) {
        const auto r = testing_helpers::run(w, name);
        sched::render_legacy(out, r, r.algorithm);
    }
    EXPECT_EQ(out.str(), slurp(std::string(SCHED_LEGACY_DIR) + "/sample_output.txt"));
}

TEST(Parse, HeaderCommentsCommasAndOptionalPriority) {
    std::istringstream in(
        "PID Arrival Burst Priority\n"
        "# a comment\n"
        "\n"
        "1 0 5 2   # trailing comment\n"
        "2,3,4,1\n"
        "3 4 2\n");
    const auto w = sched::parse_workload(in);
    ASSERT_EQ(w.size(), 3u);
    EXPECT_EQ(w[1].arrival, 3);
    EXPECT_EQ(w[1].priority, 1);
    EXPECT_EQ(w[2].priority, 0);
}

void expect_error(const std::string& text, int line, const std::string& fragment) {
    std::istringstream in(text);
    try {
        (void)sched::parse_workload(in, "w.txt");
        FAIL() << "expected a ParseError for: " << text;
    } catch (const sched::ParseError& e) {
        EXPECT_EQ(e.line(), line) << e.what();
        EXPECT_NE(std::string(e.what()).find(fragment), std::string::npos) << e.what();
        EXPECT_EQ(std::string(e.what()).rfind("w.txt:", 0), 0u) << e.what();
    }
}

TEST(Parse, ErrorsCarryLineNumbers) {
    expect_error("PID A B P\n1 0 5 1\n2 x 5 1\n", 3, "arrival is not an integer");
    expect_error("1 0 5 1\n2 1\n", 2, "expected");
    expect_error("1 0 0 1\n", 1, "burst time must be > 0");
    expect_error("1 -2 3 1\n", 1, "arrival time must be >= 0");
    expect_error("1 0 3 1\n\n1 4 2 1\n", 3, "duplicate PID 1 (first on line 1)");
    expect_error("PID Arrival Burst\n# nothing\n", 2, "no processes");
    expect_error("1 0 3 1 9\n", 1, "5 fields");
}

TEST(Parse, MissingFileThrows) {
    EXPECT_THROW((void)sched::load_workload("/no/such/file"), std::runtime_error);
}

TEST(Render, WorkloadRoundTrip) {
    const std::vector<sched::Process> w{{1, 0, 5, 2}, {7, 3, 1, 4}};
    std::stringstream s;
    sched::write_workload(s, w);
    const auto back = sched::parse_workload(s);
    ASSERT_EQ(back.size(), 2u);
    EXPECT_EQ(back[1].pid, 7);
    EXPECT_EQ(back[1].priority, 4);
}

TEST(Render, JsonCsvTextSvg) {
    const std::vector<sched::Process> w{{1, 2, 3, 1}, {2, 3, 2, 2}};
    const auto r = testing_helpers::run(w, "rr", {}, 1);
    std::ostringstream json, csv, text, cmp, svg;
    sched::render_json(json, {r});
    sched::render_csv(csv, {r});
    sched::render_text(text, r);
    sched::render_comparison(cmp, {r});
    sched::render_svg(svg, r);
    const std::string j = json.str(), c = csv.str(), t = text.str();
    EXPECT_NE(j.find("\"algorithm\": \"RR(q=2)\""), std::string::npos);
    EXPECT_NE(j.find("{\"kind\": \"idle\", \"pid\": -1, \"start\": 0, \"end\": 2}"), std::string::npos);
    EXPECT_NE(j.find("\"kind\": \"switch\""), std::string::npos);
    EXPECT_EQ(std::count(j.begin(), j.end(), '{'), std::count(j.begin(), j.end(), '}'));
    EXPECT_EQ(std::count(j.begin(), j.end(), '['), std::count(j.begin(), j.end(), ']'));
    EXPECT_EQ(std::count(c.begin(), c.end(), '\n'), 3);
    EXPECT_NE(t.find("| idle | P1 | cs | P2 | cs | P1 |"), std::string::npos) << t;
    EXPECT_NE(cmp.str().find("RR(q=2)"), std::string::npos);
    EXPECT_EQ(svg.str().rfind("<svg", 0), 0u);
    EXPECT_NE(svg.str().find("</svg>"), std::string::npos);
}

}  // namespace
