#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <algorithm>
#include <string>
using namespace std;

struct Process {
    int pid;
    int arrival;
    int burst;
    int priority;

    int remaining;
    int completion;
    int waiting;
    int turnaround;
};

struct GanttBlock {
    int pid;        // process id
    int startTime;  // block start
    int endTime;    // block end
};

static vector<Process> readProcesses(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "ERROR: Cannot open input file: " << filename << "\n";
        exit(1);
    }

    // Read and ignore header line
    string headerLine;
    getline(file, headerLine);

    vector<Process> processes;
    Process p;

    while (file >> p.pid >> p.arrival >> p.burst >> p.priority) {
        p.remaining = p.burst;
        p.completion = 0;
        p.waiting = 0;
        p.turnaround = 0;
        processes.push_back(p);
    }

    if (processes.empty()) {
        cerr << "ERROR: No process data found in file.\n";
        exit(1);
    }

    return processes;
}

static void printGantt(const vector<GanttBlock>& gantt) {
    // Top bar
    for (const auto& b : gantt) {
        cout << "| P" << b.pid << " ";
    }
    cout << "|\n";

    // Times line
    if (!gantt.empty()) {
        cout << gantt.front().startTime;
        for (const auto& b : gantt) {
            cout << "    " << b.endTime;
        }
        cout << "\n";
    }
}

static void computeAndPrintTable(vector<Process>& procs, const string& title) {
    double sumWT = 0, sumTAT = 0;

    cout << "\n" << title << " Results:\n";
    cout << "PID\tAT\tBT\tWT\tTAT\tCT\n";

    // sort by PID for clean display
    sort(procs.begin(), procs.end(), [](const Process& a, const Process& b){
        return a.pid < b.pid;
    });

    for (auto& p : procs) {
        sumWT += p.waiting;
        sumTAT += p.turnaround;
        cout << p.pid << "\t"
             << p.arrival << "\t"
             << p.burst << "\t"
             << p.waiting << "\t"
             << p.turnaround << "\t"
             << p.completion << "\n";
    }

    cout << "Average WT = " << (sumWT / procs.size()) << "\n";
    cout << "Average TAT = " << (sumTAT / procs.size()) << "\n";
}

static void runFCFS(vector<Process> procs) {
    // FCFS: sort by arrival time, tie by PID
    sort(procs.begin(), procs.end(), [](const Process& a, const Process& b){
        if (a.arrival != b.arrival) return a.arrival < b.arrival;
        return a.pid < b.pid;
    });

    int time = 0;
    vector<GanttBlock> gantt;

    for (auto& p : procs) {
        if (time < p.arrival) time = p.arrival;  // CPU idle until arrival

        int start = time;
        time += p.burst;
        int end = time;

        p.completion = end;
        p.turnaround = p.completion - p.arrival;
        p.waiting = p.turnaround - p.burst;

        gantt.push_back({p.pid, start, end});
    }

    cout << "\n============================\n";
    cout << "FCFS Gantt Chart:\n";
    printGantt(gantt);

    computeAndPrintTable(procs, "FCFS");
}

static void runRR(vector<Process> procs, int quantum) {
    // Round Robin with arrivals:
    // - We push processes into queue when they arrive.
    // - When CPU is idle, jump time to next arrival.

    // We need stable ordering by arrival then PID for initial scanning
    vector<int> idx(procs.size());
    for (int i = 0; i < (int)procs.size(); i++) idx[i] = i;

    sort(idx.begin(), idx.end(), [&](int i, int j){
        if (procs[i].arrival != procs[j].arrival) return procs[i].arrival < procs[j].arrival;
        return procs[i].pid < procs[j].pid;
    });

    queue<int> q;   // store indices into procs
    int time = 0;
    int nextArr = 0; // pointer in idx list
    int completed = 0;
    vector<GanttBlock> gantt;

    auto pushArrivalsUpToTime = [&]() {
        while (nextArr < (int)idx.size() && procs[idx[nextArr]].arrival <= time) {
            q.push(idx[nextArr]);
            nextArr++;
        }
    };

    // Start: jump to first arrival
    time = procs[idx[0]].arrival;
    pushArrivalsUpToTime();

    while (completed < (int)procs.size()) {

        if (q.empty()) {
            // CPU idle: jump to next arrival
            time = procs[idx[nextArr]].arrival;
            pushArrivalsUpToTime();
            continue;
        }

        int i = q.front();
        q.pop();

        if (procs[i].remaining <= 0) continue; // safety

        int start = time;
        int slice = min(quantum, procs[i].remaining);

        time += slice;
        procs[i].remaining -= slice;

        // Record gantt block
        gantt.push_back({procs[i].pid, start, time});

        // Add any processes that arrived during this time slice
        pushArrivalsUpToTime();

        if (procs[i].remaining > 0) {
            // not finished, back to queue
            q.push(i);
        } else {
            // finished
            procs[i].completion = time;
            procs[i].turnaround = procs[i].completion - procs[i].arrival;
            procs[i].waiting = procs[i].turnaround - procs[i].burst;
            completed++;
        }
    }

    cout << "\n============================\n";
    cout << "Round Robin (q=" << quantum << ") Gantt Chart:\n";
    printGantt(gantt);

    computeAndPrintTable(procs, "Round Robin");
}

int main(int argc, char* argv[]) {
    // Usage:
    //   ./scheduler                 -> uses processes.txt, RR quantum=2
    //   ./scheduler processes.txt 3  -> uses file, RR quantum=3

    string filename = "processes.txt";
    int quantum = 2;

    if (argc >= 2) filename = argv[1];
    if (argc >= 3) quantum = stoi(argv[2]);

    auto processes = readProcesses(filename);

    cout << "Operating System Scheduling Simulation\n";
    cout << "Input file: " << filename << "\n";

    runFCFS(processes);
    runRR(processes, quantum);

    return 0;
}
