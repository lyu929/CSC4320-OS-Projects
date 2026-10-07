package io.github.lyu929.ossync.buffer;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * The textbook solution with three semaphores: {@code empty} counts free slots, {@code full} counts
 * stored items and {@code mutex} protects the queue (this is the course project's design).
 *
 * <p>Unlike the course version, a thread interrupted between acquiring a slot permit and the mutex
 * gives the permit back, so interrupts cannot shrink the buffer.
 */
public final class SemaphoreBoundedBuffer<T> extends AbstractBoundedBuffer<T> {

    private final Semaphore empty;
    private final Semaphore full = new Semaphore(0, true);
    private final Semaphore mutex = new Semaphore(1, true);

    public SemaphoreBoundedBuffer(int capacity) {
        super(capacity);
        this.empty = new Semaphore(capacity, true);
    }

    @Override
    public void put(T item) throws InterruptedException {
        if (!empty.tryAcquire()) {
            noteFullWait();
            empty.acquire();
        }
        insertHoldingSlot(item);
    }

    @Override
    public boolean offer(T item, long timeout, TimeUnit unit) throws InterruptedException {
        if (!empty.tryAcquire()) {
            noteFullWait();
            if (!empty.tryAcquire(timeout, unit)) {
                return false;
            }
        }
        insertHoldingSlot(item);
        return true;
    }

    @Override
    public T take() throws InterruptedException {
        if (!full.tryAcquire()) {
            noteEmptyWait();
            full.acquire();
        }
        return removeHoldingItem();
    }

    @Override
    public T poll(long timeout, TimeUnit unit) throws InterruptedException {
        if (!full.tryAcquire()) {
            noteEmptyWait();
            if (!full.tryAcquire(timeout, unit)) {
                return null;
            }
        }
        return removeHoldingItem();
    }

    private void insertHoldingSlot(T item) throws InterruptedException {
        try {
            mutex.acquire();
        } catch (InterruptedException e) {
            empty.release(); // give the slot back
            throw e;
        }
        try {
            insert(item);
        } catch (RuntimeException e) {
            mutex.release();
            empty.release();
            throw e;
        }
        mutex.release();
        full.release();
    }

    private T removeHoldingItem() throws InterruptedException {
        try {
            mutex.acquire();
        } catch (InterruptedException e) {
            full.release(); // the item stays available to other consumers
            throw e;
        }
        T item;
        try {
            item = remove();
        } finally {
            mutex.release();
        }
        empty.release();
        return item;
    }

    private void noteFullWait() throws InterruptedException {
        mutex.acquire();
        try {
            countFullWait();
        } finally {
            mutex.release();
        }
    }

    private void noteEmptyWait() throws InterruptedException {
        mutex.acquire();
        try {
            countEmptyWait();
        } finally {
            mutex.release();
        }
    }

    @Override
    public int size() {
        mutex.acquireUninterruptibly();
        try {
            return items.size();
        } finally {
            mutex.release();
        }
    }

    @Override
    public BufferStats stats() {
        mutex.acquireUninterruptibly();
        try {
            return snapshot();
        } finally {
            mutex.release();
        }
    }

    @Override
    public String name() {
        return "semaphore";
    }
}
