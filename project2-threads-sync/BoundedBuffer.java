import java.util.LinkedList;
import java.util.Queue;
import java.util.concurrent.Semaphore;

public class BoundedBuffer {
    private Queue<Integer> buffer;
    private int capacity;

    private Semaphore empty;
    private Semaphore full;
    private Semaphore mutex;

    public BoundedBuffer(int capacity) {
        this.capacity = capacity;
        this.buffer = new LinkedList<>();

        empty = new Semaphore(capacity); // empty slots
        full = new Semaphore(0);         // filled slots
        mutex = new Semaphore(1);        // mutual exclusion
    }

    public void produce(int item, int producerId) throws InterruptedException {
        System.out.println("[Producer " + producerId + "] Waiting for empty slot...");
        empty.acquire();

        System.out.println("[Producer " + producerId + "] Waiting for mutex...");
        mutex.acquire();

        buffer.add(item);
        System.out.println("[Producer " + producerId + "] Produced item " + item +
                " | Buffer size = " + buffer.size());

        mutex.release();
        full.release();
    }

    public int consume(int consumerId) throws InterruptedException {
        System.out.println("[Consumer " + consumerId + "] Waiting for full slot...");
        full.acquire();

        System.out.println("[Consumer " + consumerId + "] Waiting for mutex...");
        mutex.acquire();

        int item = buffer.remove();
        System.out.println("[Consumer " + consumerId + "] Consumed item " + item +
                " | Buffer size = " + buffer.size());

        mutex.release();
        empty.release();

        return item;
    }
}