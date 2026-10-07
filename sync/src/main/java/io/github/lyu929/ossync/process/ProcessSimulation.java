package io.github.lyu929.ossync.process;

import io.github.lyu929.ossync.trace.EventLog;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.concurrent.Semaphore;

/**
 * Runs every process as a thread that arrives at its arrival time and "executes" by sleeping for its
 * burst. Time is scaled by {@code unitMillis} (the course version slept a full second per unit).
 *
 * <p>With {@code cpus > 0} a fair semaphore models a machine with that many CPUs: threads that cannot
 * get a CPU queue in arrival order, so with one CPU the run reproduces FCFS scheduling. With
 * {@code cpus == 0} every process runs immediately (unlimited CPUs, the course behaviour).
 */
public final class ProcessSimulation {

    /** Measured timings in milliseconds since the simulation started. */
    public record Run(int pid, long arrivalMillis, long startMillis, long finishMillis) {
        public long waitingMillis() {
            return startMillis - arrivalMillis;
        }
    }

    public record Result(List<Run> runs, long elapsedMillis, boolean interrupted) {
        /** PIDs in the order they got the CPU. */
        public List<Integer> startOrder() {
            return runs.stream().sorted(Comparator.comparingLong(Run::startMillis)).map(Run::pid).toList();
        }
    }

    private ProcessSimulation() {}

    public static Result run(List<ProcessSpec> processes, long unitMillis, int cpus, EventLog log)
            throws InterruptedException {
        if (unitMillis <= 0) {
            throw new IllegalArgumentException("unitMillis must be positive");
        }
        if (cpus < 0) {
            throw new IllegalArgumentException("cpus must be >= 0 (0 = unlimited)");
        }
        Semaphore cpu = cpus > 0 ? new Semaphore(cpus, true) : null;
        long t0 = System.nanoTime();
        List<Run> runs = java.util.Collections.synchronizedList(new ArrayList<>());
        List<Thread> threads = new ArrayList<>();
        boolean[] interrupted = {false};

        List<ProcessSpec> byArrival = new ArrayList<>(processes);
        byArrival.sort(Comparator.comparingInt(ProcessSpec::arrival).thenComparingInt(ProcessSpec::pid));
        for (ProcessSpec p : byArrival) {
            Thread t = new Thread(() -> {
                String me = "Process " + p.pid();
                try {
                    sleepUntil(t0, p.arrival() * unitMillis);
                    long arrived = millisSince(t0);
                    log.log(me, "arrived (burst = " + p.burst() + ")");
                    if (cpu != null) {
                        cpu.acquire();
                    }
                    long started = millisSince(t0);
                    log.log(me, "started");
                    try {
                        Thread.sleep(p.burst() * unitMillis);
                    } finally {
                        if (cpu != null) {
                            cpu.release();
                        }
                    }
                    long finished = millisSince(t0);
                    log.log(me, "finished");
                    runs.add(new Run(p.pid(), arrived, started, finished));
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt(); // restore the flag; the course code swallowed it
                    synchronized (interrupted) {
                        interrupted[0] = true;
                    }
                    log.log(me, "interrupted");
                }
            }, "process-" + p.pid());
            threads.add(t);
        }
        threads.forEach(Thread::start);
        try {
            for (Thread t : threads) {
                t.join();
            }
        } catch (InterruptedException e) {
            threads.forEach(Thread::interrupt);
            throw e;
        }
        List<Run> sorted = new ArrayList<>(runs);
        sorted.sort(Comparator.comparingInt(Run::pid));
        synchronized (interrupted) {
            return new Result(List.copyOf(sorted), millisSince(t0), interrupted[0]);
        }
    }

    private static void sleepUntil(long t0, long offsetMillis) throws InterruptedException {
        long left = offsetMillis - millisSince(t0);
        if (left > 0) {
            Thread.sleep(left);
        }
    }

    private static long millisSince(long t0) {
        return (System.nanoTime() - t0) / 1_000_000;
    }
}
