package io.github.lyu929.ossync.rw;

import java.util.concurrent.Semaphore;

/**
 * Starvation-free solution: every thread first passes a fair "service queue" semaphore, so readers
 * arriving after a waiting writer cannot overtake it, and a writer cannot overtake earlier readers.
 */
final class FairRwLock implements RwLock {

    private final Semaphore serviceQueue = new Semaphore(1, true);
    private final Semaphore resource = new Semaphore(1);
    private final Semaphore mutex = new Semaphore(1);
    private int readCount;

    @Override
    public void lockRead() throws InterruptedException {
        serviceQueue.acquire();
        try {
            mutex.acquire();
            try {
                if (++readCount == 1) {
                    try {
                        resource.acquire();
                    } catch (InterruptedException e) {
                        readCount--;
                        throw e;
                    }
                }
            } finally {
                mutex.release();
            }
        } finally {
            serviceQueue.release();
        }
    }

    @Override
    public void unlockRead() {
        mutex.acquireUninterruptibly();
        try {
            if (--readCount == 0) {
                resource.release();
            }
        } finally {
            mutex.release();
        }
    }

    @Override
    public void lockWrite() throws InterruptedException {
        serviceQueue.acquire();
        try {
            resource.acquire();
        } finally {
            serviceQueue.release();
        }
    }

    @Override
    public void unlockWrite() {
        resource.release();
    }

    @Override
    public String name() {
        return "fair";
    }
}
