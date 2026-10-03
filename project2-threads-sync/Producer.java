public class Producer extends Thread {
    private int id;
    private BoundedBuffer buffer;
    private int itemsToProduce;

    public Producer(int id, BoundedBuffer buffer, int itemsToProduce) {
        this.id = id;
        this.buffer = buffer;
        this.itemsToProduce = itemsToProduce;
    }

    @Override
    public void run() {
        for (int i = 1; i <= itemsToProduce; i++) {
            try {
                Thread.sleep(500);
                buffer.produce(i, id);
            } catch (InterruptedException e) {
                System.out.println("[Producer " + id + "] Interrupted.");
            }
        }
        System.out.println("[Producer " + id + "] Finished.");
    }
}