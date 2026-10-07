package io.github.lyu929.ossync.process;

/** One row of the workload file. Times are in abstract units. */
public record ProcessSpec(int pid, int arrival, int burst, int priority) {
    public ProcessSpec {
        if (arrival < 0) {
            throw new IllegalArgumentException("process " + pid + ": arrival must be >= 0");
        }
        if (burst <= 0) {
            throw new IllegalArgumentException("process " + pid + ": burst must be > 0");
        }
    }
}
