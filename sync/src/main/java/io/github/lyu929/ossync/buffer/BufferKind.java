package io.github.lyu929.ossync.buffer;

import java.util.Locale;

/** Factory for the three implementations. */
public enum BufferKind {
    SEMAPHORE,
    MONITOR,
    LOCK;

    public <T> BoundedBuffer<T> create(int capacity) {
        return switch (this) {
            case SEMAPHORE -> new SemaphoreBoundedBuffer<>(capacity);
            case MONITOR -> new MonitorBoundedBuffer<>(capacity);
            case LOCK -> new LockBoundedBuffer<>(capacity);
        };
    }

    public static BufferKind parse(String name) {
        try {
            return valueOf(name.trim().toUpperCase(Locale.ROOT));
        } catch (IllegalArgumentException e) {
            throw new IllegalArgumentException("unknown buffer '" + name + "' (semaphore, monitor, lock)", e);
        }
    }
}
