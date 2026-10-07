package io.github.lyu929.ossync.buffer;

import java.util.concurrent.TimeUnit;

/**
 * Java monitor version: {@code synchronized} methods with {@code wait()} in a loop (guards against
 * spurious wake-ups) and {@code notifyAll()} (producers and consumers share one wait set, so a single
 * {@code notify()} could wake the wrong kind of thread and stall everybody).
 */
public final class MonitorBoundedBuffer<T> extends AbstractBoundedBuffer<T> {

    public MonitorBoundedBuffer(int capacity) {
        super(capacity);
    }

    @Override
    public synchronized void put(T item) throws InterruptedException {
        if (items.size() == capacity) {
            countFullWait();
        }
        while (items.size() == capacity) {
            wait();
        }
        insert(item);
        notifyAll();
    }

    @Override
    public synchronized boolean offer(T item, long timeout, TimeUnit unit) throws InterruptedException {
        long deadline = System.nanoTime() + unit.toNanos(timeout);
        if (items.size() == capacity) {
            countFullWait();
        }
        while (items.size() == capacity) {
            long left = deadline - System.nanoTime();
            if (left <= 0) {
                return false;
            }
            TimeUnit.NANOSECONDS.timedWait(this, left);
        }
        insert(item);
        notifyAll();
        return true;
    }

    @Override
    public synchronized T take() throws InterruptedException {
        if (items.isEmpty()) {
            countEmptyWait();
        }
        while (items.isEmpty()) {
            wait();
        }
        T item = remove();
        notifyAll();
        return item;
    }

    @Override
    public synchronized T poll(long timeout, TimeUnit unit) throws InterruptedException {
        long deadline = System.nanoTime() + unit.toNanos(timeout);
        if (items.isEmpty()) {
            countEmptyWait();
        }
        while (items.isEmpty()) {
            long left = deadline - System.nanoTime();
            if (left <= 0) {
                return null;
            }
            TimeUnit.NANOSECONDS.timedWait(this, left);
        }
        T item = remove();
        notifyAll();
        return item;
    }

    @Override
    public synchronized int size() {
        return items.size();
    }

    @Override
    public synchronized BufferStats stats() {
        return snapshot();
    }

    @Override
    public String name() {
        return "monitor";
    }
}
