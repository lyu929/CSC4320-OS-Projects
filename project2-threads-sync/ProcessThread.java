public class ProcessThread extends Thread {
    private int pid;
    private int burstTime;

    public ProcessThread(int pid, int burstTime) {
        this.pid = pid;
        this.burstTime = burstTime;
    }

    @Override
    public void run() {
        System.out.println("[Process " + pid + "] Started. Burst time = " + burstTime + "s");
        try {
            Thread.sleep(burstTime * 1000L);
        } catch (InterruptedException e) {
            System.out.println("[Process " + pid + "] Interrupted.");
        }
        System.out.println("[Process " + pid + "] Finished.");
    }
}