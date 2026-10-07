package io.github.lyu929.ossync.rw;

/** Monitor-based writer preference: as soon as a writer is waiting, new readers queue behind it. */
final class WriterPreferenceLock implements RwLock {

    private int activeReaders;
    private boolean activeWriter;
    private int waitingWriters;

    @Override
    public synchronized void lockRead() throws InterruptedException {
        while (activeWriter || waitingWriters > 0) {
            wait();
        }
        activeReaders++;
    }

    @Override
    public synchronized void unlockRead() {
        if (--activeReaders == 0) {
            notifyAll();
        }
    }

    @Override
    public synchronized void lockWrite() throws InterruptedException {
        waitingWriters++;
        try {
            while (activeWriter || activeReaders > 0) {
                wait();
            }
        } catch (InterruptedException e) {
            waitingWriters--;
            notifyAll(); // readers blocked only because of us may proceed
            throw e;
        }
        waitingWriters--;
        activeWriter = true;
    }

    @Override
    public synchronized void unlockWrite() {
        activeWriter = false;
        notifyAll();
    }

    @Override
    public String name() {
        return "writer-preference";
    }
}
