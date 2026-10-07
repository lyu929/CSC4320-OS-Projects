package io.github.lyu929.ossync.rw;

/**
 * Readers–writer lock: any number of readers or exactly one writer.
 *
 * <p>Unlike {@link java.util.concurrent.locks.ReadWriteLock}, unlock calls may come from a different
 * thread than the matching lock call (semaphore semantics), which keeps the classic textbook
 * solutions expressible as written.
 */
public interface RwLock {

    void lockRead() throws InterruptedException;

    void unlockRead();

    void lockWrite() throws InterruptedException;

    void unlockWrite();

    String name();

    /** The three classic policies. */
    enum Policy {
        /** First readers–writers problem: readers never wait for waiting writers; writers can starve. */
        READER_PREFERENCE,
        /** Second problem: a waiting writer blocks new readers; readers can starve. */
        WRITER_PREFERENCE,
        /** Third problem: FIFO service order, nobody starves. */
        FAIR;

        public RwLock create() {
            return switch (this) {
                case READER_PREFERENCE -> new ReaderPreferenceLock();
                case WRITER_PREFERENCE -> new WriterPreferenceLock();
                case FAIR -> new FairRwLock();
            };
        }

        public static Policy parse(String s) {
            return switch (s.trim().toLowerCase(java.util.Locale.ROOT)) {
                case "reader", "readers", "reader-preference" -> READER_PREFERENCE;
                case "writer", "writers", "writer-preference" -> WRITER_PREFERENCE;
                case "fair", "fifo" -> FAIR;
                default -> throw new IllegalArgumentException("unknown policy '" + s + "' (reader, writer, fair)");
            };
        }
    }
}
