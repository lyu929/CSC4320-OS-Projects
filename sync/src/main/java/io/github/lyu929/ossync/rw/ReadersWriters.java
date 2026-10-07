package io.github.lyu929.ossync.rw;

import io.github.lyu929.ossync.trace.EventLog;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Readers and writers share a "database" (a version counter plus a value that must always equal
 * twice the version). Every access checks the exclusion invariants, so a broken lock shows up as a
 * non-zero {@code violations} count rather than as a rare corrupted value.
 */
public final class ReadersWriters {

    public record Config(RwLock.Policy policy, int readers, int writers, int opsPerThread, long readMillis,
            long writeMillis, long thinkMillis, long seed) {
        public Config {
            if (readers < 0 || writers < 0 || readers + writers == 0 || opsPerThread <= 0) {
                throw new IllegalArgumentException("need at least one thread and one operation");
            }
        }
    }

    public record Report(Config config, long reads, long writes, int violations, int maxConcurrentReaders,
            long maxReaderWaitMillis, long maxWriterWaitMillis, double meanWriterWaitMillis, long elapsedMillis) {}

    private ReadersWriters() {}

    public static Report run(Config c, EventLog log) throws InterruptedException {
        RwLock lock = c.policy().create();
        AtomicInteger activeReaders = new AtomicInteger();
        AtomicInteger activeWriters = new AtomicInteger();
        AtomicInteger violations = new AtomicInteger();
        AtomicInteger maxReaders = new AtomicInteger();
        AtomicLong reads = new AtomicLong();
        AtomicLong writes = new AtomicLong();
        AtomicLong maxReaderWait = new AtomicLong();
        AtomicLong maxWriterWait = new AtomicLong();
        AtomicLong totalWriterWait = new AtomicLong();
        long[] db = {0, 0}; // {version, value}; guarded by `lock`
        List<Thread> threads = new ArrayList<>();
        long start = System.nanoTime();

        for (int r = 1; r <= c.readers(); r++) {
            final int id = r;
            Random rng = new Random(c.seed() * 41 + id);
            threads.add(new Thread(() -> {
                String me = "Reader " + id;
                try {
                    for (int i = 0; i < c.opsPerThread(); i++) {
                        jitter(rng, c.thinkMillis());
                        long t = System.nanoTime();
                        lock.lockRead();
                        maxReaderWait.accumulateAndGet((System.nanoTime() - t) / 1_000_000, Math::max);
                        try {
                            int now = activeReaders.incrementAndGet();
                            maxReaders.accumulateAndGet(now, Math::max);
                            if (activeWriters.get() != 0) {
                                violations.incrementAndGet();
                            }
                            long version = db[0];
                            long value = db[1];
                            jitter(rng, c.readMillis());
                            if (db[0] != version || value != 2 * version) {
                                violations.incrementAndGet(); // torn or concurrent write
                            }
                            reads.incrementAndGet();
                            log.log(me, "read version " + version + " (" + now + " readers inside)");
                        } finally {
                            activeReaders.decrementAndGet();
                            lock.unlockRead();
                        }
                    }
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }, "reader-" + id));
        }
        for (int w = 1; w <= c.writers(); w++) {
            final int id = w;
            Random rng = new Random(c.seed() * 43 + 500 + id);
            threads.add(new Thread(() -> {
                String me = "Writer " + id;
                try {
                    for (int i = 0; i < c.opsPerThread(); i++) {
                        jitter(rng, c.thinkMillis());
                        long t = System.nanoTime();
                        lock.lockWrite();
                        long waited = (System.nanoTime() - t) / 1_000_000;
                        maxWriterWait.accumulateAndGet(waited, Math::max);
                        totalWriterWait.addAndGet(waited);
                        try {
                            if (activeWriters.incrementAndGet() != 1 || activeReaders.get() != 0) {
                                violations.incrementAndGet();
                            }
                            db[0]++;
                            jitter(rng, c.writeMillis());
                            db[1] = 2 * db[0];
                            writes.incrementAndGet();
                            log.log(me, "wrote version " + db[0] + " after waiting " + waited + " ms");
                        } finally {
                            activeWriters.decrementAndGet();
                            lock.unlockWrite();
                        }
                    }
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            }, "writer-" + id));
        }
        threads.forEach(Thread::start);
        for (Thread t : threads) {
            t.join();
        }
        long totalWrites = writes.get();
        return new Report(c, reads.get(), totalWrites, violations.get(), maxReaders.get(), maxReaderWait.get(),
                maxWriterWait.get(), totalWrites == 0 ? 0 : (double) totalWriterWait.get() / totalWrites,
                (System.nanoTime() - start) / 1_000_000);
    }

    private static void jitter(Random rng, long meanMillis) throws InterruptedException {
        if (meanMillis > 0) {
            Thread.sleep(Math.max(0, Math.round(meanMillis * (0.5 + rng.nextDouble()))));
        }
    }
}
