package io.github.lyu929.ossync.buffer;

import java.util.concurrent.TimeUnit;

/**
 * A fixed-capacity FIFO buffer shared by producer and consumer threads.
 *
 * <p>Implementations differ only in the synchronization primitive used (counting semaphores,
 * a Java monitor, or a lock with condition variables); they all guarantee:
 *
 * <ul>
 *   <li>never more than {@link #capacity()} items are stored;
 *   <li>{@link #take()} never returns an item twice and never loses one;
 *   <li>items are returned in insertion order;
 *   <li>a thread interrupted while blocked throws {@link InterruptedException} and leaves the buffer
 *       consistent (no leaked permits or slots).
 * </ul>
 */
public interface BoundedBuffer<T> {

    /** Insert {@code item}, blocking while the buffer is full. */
    void put(T item) throws InterruptedException;

    /** Remove the oldest item, blocking while the buffer is empty. */
    T take() throws InterruptedException;

    /** Like {@link #put} but gives up after the timeout. Returns false if the item was not inserted. */
    boolean offer(T item, long timeout, TimeUnit unit) throws InterruptedException;

    /** Like {@link #take} but gives up after the timeout and returns null. */
    T poll(long timeout, TimeUnit unit) throws InterruptedException;

    int size();

    int capacity();

    /** Counters collected inside the critical section. */
    BufferStats stats();

    /** Short name of the synchronization strategy. */
    String name();
}
