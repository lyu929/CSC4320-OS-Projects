#include <algorithm>
#include <cctype>
#include <cmath>
#include <deque>
#include <functional>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <tuple>

#include "sched/policy.hpp"

namespace sched {
namespace {

// ---------------------------------------------------------------- FCFS / RR
class Fifo : public Policy {
public:
    explicit Fifo(Time quantum = kNever) : quantum_(quantum) {}
    std::string name() const override { return quantum_ == kNever ? "FCFS" : "RR(q=" + std::to_string(quantum_) + ")"; }
    void add(Job* job, Time) override { queue_.push_back(job); }
    Job* pop(Time) override {
        if (queue_.empty()) return nullptr;
        Job* j = queue_.front();
        queue_.pop_front();
        return j;
    }
    bool empty() const override { return queue_.empty(); }
    Time time_slice(const Job&) const override { return quantum_; }

private:
    Time quantum_;
    std::deque<Job*> queue_;
};

// ------------------------------------------------- key-ordered ready queues
// Ties are broken by arrival time, then PID, so results are deterministic.
template <class Key>
class KeyedQueue {
public:
    using KeyFn = std::function<Key(const Job&)>;
    explicit KeyedQueue(KeyFn key) : key_(std::move(key)) {}
    void push(Job* j) { heap_.push({key_(*j), j->arrival, j->pid, j}); }
    Job* pop() {
        if (heap_.empty()) return nullptr;
        Job* j = std::get<3>(heap_.top());
        heap_.pop();
        return j;
    }
    [[nodiscard]] bool empty() const { return heap_.empty(); }

private:
    using Entry = std::tuple<Key, Time, int, Job*>;
    struct Greater {
        bool operator()(const Entry& a, const Entry& b) const {
            return std::tie(std::get<0>(a), std::get<1>(a), std::get<2>(a)) >
                   std::tie(std::get<0>(b), std::get<1>(b), std::get<2>(b));
        }
    };
    KeyFn key_;
    std::priority_queue<Entry, std::vector<Entry>, Greater> heap_;
};

class ShortestJob : public Policy {
public:
    explicit ShortestJob(bool preemptive)
        : preemptive_(preemptive),
          queue_(preemptive ? KeyedQueue<Time>::KeyFn([](const Job& j) { return j.remaining; })
                            : KeyedQueue<Time>::KeyFn([](const Job& j) { return j.burst; })) {}
    std::string name() const override { return preemptive_ ? "SRTF" : "SJF"; }
    void add(Job* job, Time) override { queue_.push(job); }
    Job* pop(Time) override { return queue_.pop(); }
    bool empty() const override { return queue_.empty(); }
    bool preemptive() const override { return preemptive_; }
    bool should_preempt(const Job& running, const Job& arrived, Time) const override {
        return arrived.remaining < running.remaining;  // strict: ties keep the running job
    }

private:
    bool preemptive_;
    KeyedQueue<Time> queue_;
};

// Priority scheduling with optional linear aging.
// Effective priority at time t: priority - (waited + (t - ready_since)) / aging.
// Rewritten as constant(job) - t / aging, the order between waiting jobs does not change
// over time, so a heap keyed on the constant part stays valid: O(log n) per operation.
class PriorityPolicy : public Policy {
public:
    PriorityPolicy(bool preemptive, double aging)
        : preemptive_(preemptive), aging_(aging), queue_([this](const Job& j) { return static_key(j); }) {}
    std::string name() const override {
        std::string n = preemptive_ ? "Priority-P" : "Priority";
        if (aging_ > 0) {
            std::ostringstream s;
            s << n << "(aging=" << aging_ << ")";
            return s.str();
        }
        return n;
    }
    void add(Job* job, Time) override { queue_.push(job); }
    Job* pop(Time now) override {
        Job* j = queue_.pop();
        if (j) j->dispatch_priority = effective(*j, now);
        return j;
    }
    bool empty() const override { return queue_.empty(); }
    bool preemptive() const override { return preemptive_; }
    bool should_preempt(const Job& running, const Job& arrived, Time now) const override {
        return effective(arrived, now) < running.dispatch_priority;
    }

private:
    double static_key(const Job& j) const {
        if (aging_ <= 0) return j.priority;
        return j.priority - static_cast<double>(j.waited - j.ready_since) / aging_;
    }
    double effective(const Job& j, Time now) const {
        if (aging_ <= 0) return j.priority;
        return j.priority - static_cast<double>(j.waited + (now - j.ready_since)) / aging_;
    }
    bool preemptive_;
    double aging_;
    KeyedQueue<double> queue_;
};

// Highest response ratio next: (waiting + burst) / burst, non-preemptive. O(n) selection.
class Hrrn : public Policy {
public:
    std::string name() const override { return "HRRN"; }
    void add(Job* job, Time) override { ready_.push_back(job); }
    Job* pop(Time now) override {
        if (ready_.empty()) return nullptr;
        auto ratio = [now](const Job* j) {
            return static_cast<double>(now - j->arrival + j->burst) / static_cast<double>(j->burst);
        };
        auto best = std::max_element(ready_.begin(), ready_.end(), [&](const Job* a, const Job* b) {
            const double ra = ratio(a), rb = ratio(b);
            if (ra != rb) return ra < rb;
            return std::tie(b->arrival, b->pid) < std::tie(a->arrival, a->pid);  // earlier wins
        });
        Job* j = *best;
        ready_.erase(best);
        return j;
    }
    bool empty() const override { return ready_.empty(); }

private:
    std::vector<Job*> ready_;
};

// Multi-level feedback queue (OSTEP rules 1-5):
//  * new jobs enter the top level; higher levels always run first (preemptive);
//  * a job that uses up its allotment at a level is demoted (the allotment is kept
//    across voluntary preemptions, so a job cannot game the scheduler);
//  * the lowest level is round robin with its own quantum;
//  * every `boost` time units all jobs move back to the top level (no starvation).
class Mlfq : public Policy {
public:
    Mlfq(std::vector<Time> quanta, Time boost) : quanta_(std::move(quanta)), boost_(boost), levels_(quanta_.size()) {}
    std::string name() const override {
        std::ostringstream s;
        s << "MLFQ(q=";
        for (std::size_t i = 0; i < quanta_.size(); ++i) s << (i ? "/" : "") << quanta_[i];
        if (boost_ > 0) s << ",boost=" << boost_;
        s << ")";
        return s.str();
    }
    void add(Job* job, Time) override { levels_[static_cast<std::size_t>(job->level)].push_back(job); }
    Job* pop(Time) override {
        for (auto& q : levels_) {
            if (!q.empty()) {
                Job* j = q.front();
                q.pop_front();
                return j;
            }
        }
        return nullptr;
    }
    bool empty() const override {
        return std::all_of(levels_.begin(), levels_.end(), [](const auto& q) { return q.empty(); });
    }
    Time time_slice(const Job& j) const override {
        return quanta_[static_cast<std::size_t>(j.level)] - j.used_at_level;
    }
    bool preemptive() const override { return true; }
    bool should_preempt(const Job& running, const Job& arrived, Time) const override {
        return arrived.level < running.level;
    }
    void on_ran(Job& job, Time ran) override { job.used_at_level += ran; }
    void on_slice_expired(Job& job, Time) override {
        if (job.level + 1 < static_cast<int>(levels_.size())) ++job.level;
        job.used_at_level = 0;
    }
    Time next_event(Time) const override { return boost_ > 0 ? last_boost_ + boost_ : kNever; }
    bool on_event(Time now, Job* running) override {
        last_boost_ = now - now % boost_;
        std::deque<Job*> all;
        for (auto& q : levels_) {
            for (Job* j : q) {
                j->level = 0;
                j->used_at_level = 0;
                all.push_back(j);
            }
            q.clear();
        }
        levels_[0] = std::move(all);
        if (running != nullptr) {
            running->level = 0;
            running->used_at_level = 0;
            return true;  // re-queue behind the boosted jobs
        }
        return false;
    }

private:
    std::vector<Time> quanta_;
    Time boost_;
    Time last_boost_ = 0;
    std::vector<std::deque<Job*>> levels_;
};

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

}  // namespace

std::vector<std::string> policy_names() {
    return {"fcfs", "sjf", "srtf", "hrrn", "prio", "prio-p", "rr", "mlfq"};
}

std::unique_ptr<Policy> make_policy(const std::string& raw_name, const PolicyConfig& cfg) {
    const std::string name = lower(raw_name);
    if (name == "fcfs") return std::make_unique<Fifo>();
    if (name == "rr") {
        if (cfg.quantum <= 0) throw std::invalid_argument("RR quantum must be positive");
        return std::make_unique<Fifo>(cfg.quantum);
    }
    if (name == "sjf") return std::make_unique<ShortestJob>(false);
    if (name == "srtf") return std::make_unique<ShortestJob>(true);
    if (name == "hrrn") return std::make_unique<Hrrn>();
    if (name == "prio" || name == "priority" || name == "prio-p" || name == "priority-p") {
        if (cfg.aging < 0) throw std::invalid_argument("aging must be >= 0");
        return std::make_unique<PriorityPolicy>(name.back() == 'p', cfg.aging);
    }
    if (name == "mlfq") {
        if (cfg.mlfq_quanta.empty()) throw std::invalid_argument("MLFQ needs at least one level");
        for (Time q : cfg.mlfq_quanta) {
            if (q <= 0) throw std::invalid_argument("MLFQ quanta must be positive");
        }
        if (cfg.mlfq_boost < 0) throw std::invalid_argument("MLFQ boost period must be >= 0");
        return std::make_unique<Mlfq>(cfg.mlfq_quanta, cfg.mlfq_boost);
    }
    throw std::invalid_argument("unknown policy '" + raw_name + "'");
}

}  // namespace sched
