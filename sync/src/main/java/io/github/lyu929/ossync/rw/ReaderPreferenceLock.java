package io.github.lyu929.ossync.rw;

import java.util.concurrent.Semaphore;

/** Silberschatz's first readers–writers solution: {@code rwMutex} + {@code mutex} + a reader count. */
final class ReaderPreferenceLock implements RwLock {

    private final Semaphore rwMutex = new Semaphore(1);
    private final Semaphore mutex = new Semaphore(1);
    private int readCount;

    @Override
    public void lockRead() throws InterruptedException {
        mutex.acquire();
        try {
            if (++readCount == 1) {
                try {
                    rwMutex.acquire(); // the first reader locks writers out
                } catch (InterruptedException e) {
                    readCount--;
                    throw e;
                }
            }
        } finally {
            mutex.release();
        }
    }

    @Override
    public void unlockRead() {
        mutex.acquireUninterruptibly();
        try {
            if (--readCount == 0) {
                rwMutex.release(); // the last reader lets writers in
            }
        } finally {
            mutex.release();
        }
    }

    @Override
    public void lockWrite() throws InterruptedException {
        rwMutex.acquire();
    }

    @Override
    public void unlockWrite() {
        rwMutex.release();
    }

    @Override
    public String name() {
        return "reader-preference";
    }
}
