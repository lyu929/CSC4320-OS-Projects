package io.github.lyu929.ossync.buffer;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.ReentrantLock;

/**
 * {@link ReentrantLock} with two condition variables, so producers wait on {@code notFull} and
 * consumers on {@code notEmpty} and a single {@code signal()} always wakes the right kind of thread.
 */
public final class LockBoundedBuffer<T> extends AbstractBoundedBuffer<T> {

    private final ReentrantLock lock;
    private final Condition notFull;
    private final Condition notEmpty;

    public LockBoundedBuffer(int capacity) {
        this(capacity, true);
    }

    public LockBoundedBuffer(int capacity, boolean fair) {
        super(capacity);
        this.lock = new ReentrantLock(fair);
        this.notFull = lock.newCondition();
        this.notEmpty = lock.newCondition();
    }

    @Override
    public void put(T item) throws InterruptedException {
        lock.lockInterruptibly();
        try {
            if (items.size() == capacity) {
                countFullWait();
            }
            while (items.size() == capacity) {
                notFull.await();
            }
            insert(item);
            notEmpty.signal();
        } finally {
            lock.unlock();
        }
    }

    @Override
    public boolean offer(T item, long timeout, TimeUnit unit) throws InterruptedException {
        long nanos = unit.toNanos(timeout);
        lock.lockInterruptibly();
        try {
            if (items.size() == capacity) {
                countFullWait();
            }
            while (items.size() == capacity) {
                if (nanos <= 0) {
                    return false;
                }
                nanos = notFull.awaitNanos(nanos);
            }
            insert(item);
            notEmpty.signal();
            return true;
        } finally {
            lock.unlock();
        }
    }

    @Override
    public T take() throws InterruptedException {
        lock.lockInterruptibly();
        try {
            if (items.isEmpty()) {
                countEmptyWait();
            }
            while (items.isEmpty()) {
                notEmpty.await();
            }
            T item = remove();
            notFull.signal();
            return item;
        } finally {
            lock.unlock();
        }
    }

    @Override
    public T poll(long timeout, TimeUnit unit) throws InterruptedException {
        long nanos = unit.toNanos(timeout);
        lock.lockInterruptibly();
        try {
            if (items.isEmpty()) {
                countEmptyWait();
            }
            while (items.isEmpty()) {
                if (nanos <= 0) {
                    return null;
                }
                nanos = notEmpty.awaitNanos(nanos);
            }
            T item = remove();
            notFull.signal();
            return item;
        } finally {
            lock.unlock();
        }
    }

    @Override
    public int size() {
        lock.lock();
        try {
            return items.size();
        } finally {
            lock.unlock();
        }
    }

    @Override
    public BufferStats stats() {
        lock.lock();
        try {
            return snapshot();
        } finally {
            lock.unlock();
        }
    }

    @Override
    public String name() {
        return "lock";
    }
}
