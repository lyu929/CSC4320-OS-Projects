import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.Scanner;

public class Main {
    public static void main(String[] args) {
        ArrayList<ProcessThread> processList = new ArrayList<>();

        // =========================
        // Part 1: Process Simulation
        // =========================
        try {
            Scanner fileScanner = new Scanner(new File("processes.txt"));

            // Skip header line
            if (fileScanner.hasNextLine()) {
                fileScanner.nextLine();
            }

            while (fileScanner.hasNextLine()) {
                String line = fileScanner.nextLine().trim();

                if (line.isEmpty()) {
                    continue;
                }

                String[] parts = line.split("\\s+");

                int pid = Integer.parseInt(parts[0]);
                int burst = Integer.parseInt(parts[2]); // PID Arrival Burst Priority

                processList.add(new ProcessThread(pid, burst));
            }

            fileScanner.close();
        } catch (FileNotFoundException e) {
            System.out.println("Error: processes.txt not found.");
            return;
        } catch (Exception e) {
            System.out.println("Error reading processes.txt: " + e.getMessage());
            return;
        }

        System.out.println("====================================");
        System.out.println("Starting Process Simulation");
        System.out.println("====================================");

        for (ProcessThread p : processList) {
            p.start();
        }

        for (ProcessThread p : processList) {
            try {
                p.join();
            } catch (InterruptedException e) {
                System.out.println("A process thread was interrupted.");
            }
        }

        System.out.println("====================================");
        System.out.println("Process Simulation Complete");
        System.out.println("====================================\n");

        // ==================================
        // Part 2: Producer-Consumer Problem
        // ==================================
        System.out.println("====================================");
        System.out.println("Starting Producer-Consumer Simulation");
        System.out.println("====================================");

        BoundedBuffer buffer = new BoundedBuffer(3);

        Producer producer1 = new Producer(1, buffer, 10);
        Consumer consumer1 = new Consumer(1, buffer, 5);
        Consumer consumer2 = new Consumer(2, buffer, 5);

        producer1.start();
        consumer1.start();
        consumer2.start();

        try {
            producer1.join();
            consumer1.join();
            consumer2.join();
        } catch (InterruptedException e) {
            System.out.println("A producer/consumer thread was interrupted.");
        }

        System.out.println("====================================");
        System.out.println("Producer-Consumer Simulation Complete");
        System.out.println("====================================");
    }
}