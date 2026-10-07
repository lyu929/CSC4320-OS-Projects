package io.github.lyu929.ossync.dining;

import io.github.lyu929.ossync.trace.EventLog;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Random;
import java.util.concurrent.BrokenBarrierException;
import java.util.concurrent.CyclicBarrier;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicIntegerArray;
import java.util.concurrent.locks.ReentrantLock;

/**
 * Dining philosophers with four strategies. Philosopher {@code i} needs forks {@code i} (left) and
 * {@code (i + 1) % n} (right).
 *
 * <ul>
 *   <li>{@link Strategy#NAIVE} – left then right; deadlocks when everybody holds a left fork.
 *   <li>{@link Strategy#ORDERED} – always take the lower-numbered fork first (breaks circular wait).
 *   <li>{@link Strategy#WAITER} – a semaphore admits at most n−1 philosophers to the table.
 *   <li>{@link Strategy#TRY_LOCK} – give the left fork back if the right one is busy, then back off
 *       (breaks hold-and-wait).
 * </ul>
 *
 * <p>A watchdog detects deadlock: if the run has not finished within {@code timeoutMillis}, every
 * thread is interrupted and the report says {@code deadlocked = true}.
 */
public final class DiningPhilosophers {

    public enum Strategy {
        NAIVE, ORDERED, WAITER, TRY_LOCK;

        public static Strategy parse(String s) {
            try {
                return valueOf(s.trim().toUpperCase(Locale.ROOT).replace('-', '_'));
            } catch (IllegalArgumentException e) {
                throw new IllegalArgumentException("unknown strategy '" + s + "' (naive, ordered, waiter, try-lock)", e);
            }
        }
    }

    /**
     * @param forceDeadlock NAIVE only: every philosopher waits at a barrier after taking the left
     *     fork, which makes the circular wait (and therefore the deadlock) certain
     */
    public record Config(int philosophers, int meals, long eatMillis, long thinkMillis, Strategy strategy,
            long timeoutMillis, boolean forceDeadlock, long seed) {
        public Config {
            if (philosophers < 2 || meals < 0 || timeoutMillis <= 0) {
                throw new IllegalArgumentException("need >= 2 philosophers, meals >= 0 and a positive timeout");
            }
            if (forceDeadlock && strategy != Strategy.NAIVE) {
                throw new IllegalArgumentException("forceDeadlock only makes sense for the NAIVE strategy");
            }
        }
    }

    /**
     * @param meals meals eaten by each philosopher
     * @param neighbourViolations times two neighbours were observed eating at once (must be 0)
     */
    public record Report(Config config, int[] meals, boolean deadlocked, int neighbourViolations,
            int maxEatingAtOnce, long elapsedMillis) {
        public int totalMeals() {
            int s = 0;
            for (int m : meals) {
                s += m;
            }
            return s;
        }
    }

    private DiningPhilosophers() {}

    public static Report run(Config c, EventLog log) throws InterruptedException {
        int n = c.philosophers();
        ReentrantLock[] forks = new ReentrantLock[n];
        for (int i = 0; i < n; i++) {
            forks[i] = new ReentrantLock(true);
        }
        Semaphore waiter = new Semaphore(n - 1, true);
        CyclicBarrier holdingLeft = new CyclicBarrier(n);
        AtomicIntegerArray eating = new AtomicIntegerArray(n);
        AtomicIntegerArray meals = new AtomicIntegerArray(n);
        AtomicInteger violations = new AtomicInteger();
        AtomicInteger eatingNow = new AtomicInteger();
        AtomicInteger maxEating = new AtomicInteger();
        List<Thread> threads = new ArrayList<>();
        long start = System.nanoTime();

        for (int i = 0; i < n; i++) {
            final int me = i;
            final int left = i;
            final int right = (i + 1) % n;
            Random rng = new Random(c.seed() * 53 + i);
            threads.add(new Thread(() -> {
                String who = "Philosopher " + me;
                try {
                    for (int meal = 0; meal < c.meals(); meal++) {
                        pause(rng, c.thinkMillis());
                        log.log(who, "hungry");
                        boolean first = meal == 0;
                        acquire(c, forks, waiter, holdingLeft, left, right, rng, first);
                        try {
                            int now = eatingNow.incrementAndGet();
                            maxEating.accumulateAndGet(now, Math::max);
                            eating.set(me, 1);
                            if (eating.get((me + n - 1) % n) == 1 || eating.get((me + 1) % n) == 1) {
                                violations.incrementAndGet();
                            }
                            log.log(who, "eating (meal " + (meal + 1) + ")");
                            pause(rng, c.eatMillis());
                            meals.incrementAndGet(me);
                            eating.set(me, 0);
                            eatingNow.decrementAndGet();
                        } finally {
                            forks[left].unlock();
                            forks[right].unlock();
                            if (c.strategy() == Strategy.WAITER) {
                                waiter.release();
                            }
                        }
                    }
                    log.log(who, "done");
                } catch (InterruptedException | BrokenBarrierException e) {
                    Thread.currentThread().interrupt();
                    log.log(who, "interrupted by the watchdog, forks released");
                }
            }, "philosopher-" + i));
        }
        threads.forEach(Thread::start);
        long deadline = System.currentTimeMillis() + c.timeoutMillis();
        boolean finished = true;
        for (Thread t : threads) {
            long left = deadline - System.currentTimeMillis();
            if (left > 0) {
                t.join(left);
            }
            if (t.isAlive()) {
                finished = false;
            }
        }
        if (!finished) {
            log.log("Watchdog", "no progress within " + c.timeoutMillis() + " ms: deadlock, interrupting everyone");
            threads.forEach(Thread::interrupt);
            for (Thread t : threads) {
                t.join();
            }
        }
        int[] m = new int[n];
        for (int i = 0; i < n; i++) {
            m[i] = meals.get(i);
        }
        return new Report(c, m, !finished, violations.get(), maxEating.get(), (System.nanoTime() - start) / 1_000_000);
    }

    private static void acquire(Config c, ReentrantLock[] forks, Semaphore waiter, CyclicBarrier holdingLeft, int left,
            int right, Random rng, boolean firstMeal) throws InterruptedException, BrokenBarrierException {
        switch (c.strategy()) {
            case NAIVE -> {
                forks[left].lockInterruptibly();
                try {
                    if (c.forceDeadlock() && firstMeal) {
                        holdingLeft.await();
                    }
                    forks[right].lockInterruptibly();
                } catch (InterruptedException | BrokenBarrierException e) {
                    forks[left].unlock();
                    throw e;
                }
            }
            case ORDERED -> {
                int lo = Math.min(left, right);
                int hi = Math.max(left, right);
                forks[lo].lockInterruptibly();
                try {
                    forks[hi].lockInterruptibly();
                } catch (InterruptedException e) {
                    forks[lo].unlock();
                    throw e;
                }
            }
            case WAITER -> {
                waiter.acquire();
                try {
                    forks[left].lockInterruptibly();
                    try {
                        forks[right].lockInterruptibly();
                    } catch (InterruptedException e) {
                        forks[left].unlock();
                        throw e;
                    }
                } catch (InterruptedException e) {
                    waiter.release();
                    throw e;
                }
            }
            case TRY_LOCK -> {
                while (true) {
                    forks[left].lockInterruptibly();
                    boolean gotRight;
                    try {
                        gotRight = forks[right].tryLock(1 + rng.nextInt(5), TimeUnit.MILLISECONDS);
                    } catch (InterruptedException e) {
                        forks[left].unlock();
                        throw e;
                    }
                    if (gotRight) {
                        return;
                    }
                    forks[left].unlock();
                    Thread.sleep(1 + rng.nextInt(10)); // randomised back-off avoids livelock
                }
            }
            default -> throw new IllegalStateException("unexpected strategy " + c.strategy());
        }
    }

    private static void pause(Random rng, long meanMillis) throws InterruptedException {
        if (meanMillis > 0) {
            Thread.sleep(Math.max(0, Math.round(meanMillis * (0.5 + rng.nextDouble()))));
        }
    }
}
