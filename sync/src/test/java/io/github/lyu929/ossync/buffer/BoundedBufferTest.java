package io.github.lyu929.ossync.buffer;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNull;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.Timeout;

@Timeout(value = 60, unit = TimeUnit.SECONDS)
class BoundedBufferTest {

    @Test
    void singleProducerSingleConsumerPreservesOrder() throws Exception {
        for (BufferKind kind : BufferKind.values()) {
            BoundedBuffer<Integer> buf = kind.create(4);
            int n = 2000;
            Thread producer = new Thread(() -> {
                try {
                    for (int i = 0; i < n; i++) {
                        buf.put(i);
                    }
                } catch (InterruptedException e) {
                    Thread.currentThread().interrupt();
                }
            });
            producer.start();
            for (int i = 0; i < n; i++) {
                assertEquals(i, (int) buf.take(), kind + " returned items out of order");
            }
            producer.join();
            BufferStats s = buf.stats();
            assertEquals(n, s.puts(), kind.name());
            assertEquals(n, s.takes(), kind.name());
            assertTrue(s.maxOccupancy() <= 4, kind + " exceeded its capacity");
            assertEquals(0, buf.size());
        }
    }

    @Test
    void manyProducersAndConsumersLoseAndDuplicateNothing() throws Exception {
        for (BufferKind kind : BufferKind.values()) {
            BoundedBuffer<int[]> buf = kind.create(3);
            int producers = 4;
            int consumers = 3;
            int perProducer = 1500;
            List<List<int[]>> taken = new ArrayList<>();
            List<Thread> threads = new ArrayList<>();
            for (int p = 0; p < producers; p++) {
                final int id = p;
                threads.add(new Thread(() -> {
                    try {
                        for (int i = 0; i < perProducer; i++) {
                            buf.put(new int[] {id, i});
                        }
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                    }
                }));
            }
            int total = producers * perProducer;
            CountDownLatch remaining = new CountDownLatch(total);
            for (int c = 0; c < consumers; c++) {
                List<int[]> mine = Collections.synchronizedList(new ArrayList<>());
                taken.add(mine);
                Thread t = new Thread(() -> {
                    try {
                        while (true) {
                            int[] item = buf.poll(50, TimeUnit.MILLISECONDS);
                            if (item != null) {
                                mine.add(item);
                                remaining.countDown();
                            } else if (remaining.getCount() == 0) {
                                return;
                            }
                        }
                    } catch (InterruptedException e) {
                        Thread.currentThread().interrupt();
                    }
                });
                t.setDaemon(true);
                threads.add(t);
            }
            threads.forEach(Thread::start);
            assertTrue(remaining.await(30, TimeUnit.SECONDS), kind + " stalled");
            for (Thread t : threads) {
                t.join(5000);
            }
            Map<Integer, Integer> lastSeq = new HashMap<>();
            boolean[][] seen = new boolean[producers][perProducer];
            int count = 0;
            for (List<int[]> mine : taken) {
                lastSeq.clear();
                for (int[] item : mine) {
                    assertFalse(seen[item[0]][item[1]], kind + " delivered an item twice");
                    seen[item[0]][item[1]] = true;
                    count++;
                    // FIFO: each consumer sees every producer's items in increasing order
                    assertTrue(lastSeq.getOrDefault(item[0], -1) < item[1], kind + " broke FIFO order");
                    lastSeq.put(item[0], item[1]);
                }
            }
            assertEquals(total, count, kind + " lost items");
            assertTrue(buf.stats().maxOccupancy() <= 3, kind.name());
        }
    }

    @Test
    void timeoutsReturnInsteadOfBlocking() throws Exception {
        for (BufferKind kind : BufferKind.values()) {
            BoundedBuffer<String> buf = kind.create(1);
            assertNull(buf.poll(20, TimeUnit.MILLISECONDS), kind.name());
            assertTrue(buf.offer("a", 20, TimeUnit.MILLISECONDS));
            long t = System.nanoTime();
            assertFalse(buf.offer("b", 30, TimeUnit.MILLISECONDS), kind + " accepted beyond capacity");
            assertTrue(System.nanoTime() - t >= TimeUnit.MILLISECONDS.toNanos(25), kind + " did not wait");
            assertEquals("a", buf.take());
            assertTrue(buf.stats().fullWaits() >= 1 && buf.stats().emptyWaits() >= 1, kind.name());
        }
    }

    @Test
    void interruptedProducerDoesNotCorruptTheBuffer() throws Exception {
        for (BufferKind kind : BufferKind.values()) {
            BoundedBuffer<Integer> buf = kind.create(2);
            buf.put(1);
            buf.put(2);
            AtomicReference<Throwable> thrown = new AtomicReference<>();
            Thread blocked = new Thread(() -> {
                try {
                    buf.put(3);
                } catch (Throwable e) {
                    thrown.set(e);
                }
            });
            blocked.start();
            Thread.sleep(50);
            blocked.interrupt();
            blocked.join(2000);
            assertTrue(thrown.get() instanceof InterruptedException, kind + ": " + thrown.get());
            // capacity is still exactly 2: one take frees exactly one slot
            assertEquals(1, (int) buf.take());
            assertTrue(buf.offer(4, 10, TimeUnit.MILLISECONDS));
            assertFalse(buf.offer(5, 10, TimeUnit.MILLISECONDS), kind + " leaked a slot");
            assertEquals(2, (int) buf.take());
            assertEquals(4, (int) buf.take());
            assertNull(buf.poll(10, TimeUnit.MILLISECONDS), kind + " invented an item");
        }
    }

    @Test
    void invalidUseIsRejected() throws Exception {
        for (BufferKind kind : BufferKind.values()) {
            assertThrows(IllegalArgumentException.class, () -> kind.create(0));
            BoundedBuffer<String> buf = kind.create(1);
            assertThrows(NullPointerException.class, () -> buf.put(null));
            assertTrue(buf.offer("still usable", 10, TimeUnit.MILLISECONDS), kind + " broken after a failed put");
        }
        assertEquals(BufferKind.LOCK, BufferKind.parse(" Lock "));
        assertThrows(IllegalArgumentException.class, () -> BufferKind.parse("spinlock"));
    }
}
