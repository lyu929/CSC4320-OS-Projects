package io.github.lyu929.ossync.buffer;

import io.github.lyu929.ossync.trace.EventLog;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Random;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.TimeUnit;

/**
 * Producer–consumer simulation over any {@link BoundedBuffer}.
 *
 * <p>Producers each create {@code itemsPerProducer} items. When every producer has finished, one
 * "poison pill" per consumer is enqueued, so consumers stop by themselves no matter how the work was
 * distributed (the course version hard-coded how many items each consumer would take, which deadlocks
 * as soon as the counts do not add up).
 */
public final class ProducerConsumer {

    /** An item tagged with its producer and per-producer sequence number. */
    public record Item(int producer, int seq) {
        static final Item POISON = new Item(-1, -1);

        @Override
        public String toString() {
            return producer + "-" + seq;
        }
    }

    /**
     * @param kind buffer implementation
     * @param capacity buffer slots
     * @param producers number of producer threads
     * @param consumers number of consumer threads
     * @param itemsPerProducer items each producer creates
     * @param produceMillis mean pause before each put (uniformly jittered ±50%)
     * @param consumeMillis mean pause after each take (uniformly jittered ±50%)
     * @param seed seed for the jitter
     * @param timeoutMillis give up (interrupt everyone) after this long; 0 = no limit
     */
    public record Config(BufferKind kind, int capacity, int producers, int consumers, int itemsPerProducer,
            long produceMillis, long consumeMillis, long seed, long timeoutMillis) {
        public Config {
            if (capacity <= 0 || producers <= 0 || consumers <= 0 || itemsPerProducer < 0) {
                throw new IllegalArgumentException("capacity, producers and consumers must be positive");
            }
            if (produceMillis < 0 || consumeMillis < 0 || timeoutMillis < 0) {
                throw new IllegalArgumentException("delays and timeout must be >= 0");
            }
        }

        /** The course scenario: one producer, two consumers, ten items, three slots (delays scaled 1/10). */
        public static Config course(BufferKind kind) {
            return new Config(kind, 3, 1, 2, 10, 50, 80, 1, 30_000);
        }
    }

    /**
     * @param produced every item handed to the buffer, in put order per producer
     * @param consumed every item taken (excluding poison), in global take order
     * @param perConsumer items taken by each consumer, in that consumer's order
     */
    public record Report(Config config, List<Item> produced, List<Item> consumed, List<List<Item>> perConsumer,
            BufferStats stats, long elapsedMillis, boolean completed) {}

    private ProducerConsumer() {}

    public static Report run(Config c, EventLog log) throws InterruptedException {
        BoundedBuffer<Item> buffer = c.kind().create(c.capacity());
        ConcurrentLinkedQueue<Item> produced = new ConcurrentLinkedQueue<>();
        ConcurrentLinkedQueue<Item> consumed = new ConcurrentLinkedQueue<>();
        List<List<Item>> perConsumer = new ArrayList<>();
        List<Thread> producers = new ArrayList<>();
        List<Thread> consumers = new ArrayList<>();
        long start = System.nanoTime();

        for (int p = 1; p <= c.producers(); p++) {
            final int id = p;
            Random rng = new Random(c.seed() * 31 + id);
            producers.add(new Thread(() -> {
                String me = "Producer " + id;
                try {
                    for (int i = 1; i <= c.itemsPerProducer(); i++) {
                        pause(rng, c.produceMillis());
                        Item item = new Item(id, i);
                        log.log(me, "waiting for an empty slot");
                        buffer.put(item);
                        produced.add(item);
                        log.log(me, "produced item " + item + " | buffer size = " + buffer.size());
                    }
                    log.log(me, "finished");
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    log.log(me, "interrupted");
                }
            }, "producer-" + id));
        }
        for (int k = 1; k <= c.consumers(); k++) {
            final int id = k;
            Random rng = new Random(c.seed() * 37 + 1000 + id);
            List<Item> mine = Collections.synchronizedList(new ArrayList<>());
            perConsumer.add(mine);
            consumers.add(new Thread(() -> {
                String me = "Consumer " + id;
                try {
                    while (true) {
                        log.log(me, "waiting for a full slot");
                        Item item = buffer.take();
                        if (item.equals(Item.POISON)) {
                            break;
                        }
                        consumed.add(item);
                        mine.add(item);
                        log.log(me, "consumed item " + item + " | buffer size = " + buffer.size());
                        pause(rng, c.consumeMillis());
                    }
                    log.log(me, "finished");
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                    log.log(me, "interrupted");
                }
            }, "consumer-" + id));
        }

        producers.forEach(Thread::start);
        consumers.forEach(Thread::start);
        long deadline = c.timeoutMillis() == 0 ? Long.MAX_VALUE : System.currentTimeMillis() + c.timeoutMillis();
        boolean ok = joinAll(producers, deadline);
        if (ok) {
            for (int k = 0; k < c.consumers(); k++) {
                long left = deadline - System.currentTimeMillis();
                if (left <= 0 || !buffer.offer(Item.POISON, left, TimeUnit.MILLISECONDS)) {
                    ok = false;
                    break;
                }
            }
        }
        ok = ok && joinAll(consumers, deadline);
        if (!ok) {
            producers.forEach(Thread::interrupt);
            consumers.forEach(Thread::interrupt);
            joinAll(producers, Long.MAX_VALUE);
            joinAll(consumers, Long.MAX_VALUE);
        }
        long elapsed = (System.nanoTime() - start) / 1_000_000;
        List<List<Item>> frozen = new ArrayList<>();
        for (List<Item> l : perConsumer) {
            synchronized (l) {
                frozen.add(List.copyOf(l));
            }
        }
        return new Report(c, List.copyOf(produced), List.copyOf(consumed), frozen, buffer.stats(), elapsed, ok);
    }

    static void pause(Random rng, long meanMillis) throws InterruptedException {
        if (meanMillis > 0) {
            Thread.sleep(Math.max(0, Math.round(meanMillis * (0.5 + rng.nextDouble()))));
        }
    }

    private static boolean joinAll(List<Thread> threads, long deadline) throws InterruptedException {
        for (Thread t : threads) {
            long left = deadline - System.currentTimeMillis();
            if (deadline == Long.MAX_VALUE) {
                t.join();
            } else if (left <= 0) {
                return false;
            } else {
                t.join(left);
            }
            if (t.isAlive()) {
                return false;
            }
        }
        return true;
    }
}
