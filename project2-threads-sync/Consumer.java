public class Consumer extends Thread {
    private int id;
    private BoundedBuffer buffer;
    private int itemsToConsume;

    public Consumer(int id, BoundedBuffer buffer, int itemsToConsume) {
        this.id = id;
        this.buffer = buffer;
        this.itemsToConsume = itemsToConsume;
    }

    @Override
    public void run() {
        for (int i = 1; i <= itemsToConsume; i++) {
            try {
                Thread.sleep(800);
                buffer.consume(id);
            } catch (InterruptedException e) {
                System.out.println("[Consumer " + id + "] Interrupted.");
            }
        }
        System.out.println("[Consumer " + id + "] Finished.");
    }
}