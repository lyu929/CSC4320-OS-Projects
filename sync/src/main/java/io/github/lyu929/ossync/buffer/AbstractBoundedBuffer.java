package io.github.lyu929.ossync.buffer;

import java.util.ArrayDeque;
import java.util.Objects;

/** Storage and statistics shared by all implementations. Callers must hold the implementation's lock. */
abstract class AbstractBoundedBuffer<T> implements BoundedBuffer<T> {

    protected final int capacity;
    protected final ArrayDeque<T> items;
    private long puts;
    private long takes;
    private int maxOccupancy;
    private long fullWaits;
    private long emptyWaits;

    AbstractBoundedBuffer(int capacity) {
        if (capacity <= 0) {
            throw new IllegalArgumentException("capacity must be positive, got " + capacity);
        }
        this.capacity = capacity;
        this.items = new ArrayDeque<>(capacity);
    }

    /** Insert under the lock; checks the capacity invariant. */
    protected final void insert(T item) {
        Objects.requireNonNull(item, "item");
        if (items.size() >= capacity) {
            throw new IllegalStateException(name() + ": capacity invariant violated");
        }
        items.addLast(item);
        puts++;
        maxOccupancy = Math.max(maxOccupancy, items.size());
    }

    /** Remove under the lock; checks the non-empty invariant. */
    protected final T remove() {
        T item = items.pollFirst();
        if (item == null) {
            throw new IllegalStateException(name() + ": took from an empty buffer");
        }
        takes++;
        return item;
    }

    protected final void countFullWait() {
        fullWaits++;
    }

    protected final void countEmptyWait() {
        emptyWaits++;
    }

    protected final BufferStats snapshot() {
        return new BufferStats(puts, takes, maxOccupancy, fullWaits, emptyWaits);
    }

    @Override
    public final int capacity() {
        return capacity;
    }
}
