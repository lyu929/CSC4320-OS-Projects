package io.github.lyu929.ossync.buffer;

/**
 * Counters observed inside a buffer's critical section.
 *
 * @param puts items inserted
 * @param takes items removed
 * @param maxOccupancy largest number of items ever stored at once
 * @param fullWaits times a producer found the buffer full and had to wait
 * @param emptyWaits times a consumer found the buffer empty and had to wait
 */
public record BufferStats(long puts, long takes, int maxOccupancy, long fullWaits, long emptyWaits) {}
