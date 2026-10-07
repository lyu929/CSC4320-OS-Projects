package io.github.lyu929.ossync;

import io.github.lyu929.ossync.buffer.BufferKind;
import io.github.lyu929.ossync.buffer.ProducerConsumer;
import io.github.lyu929.ossync.dining.DiningPhilosophers;
import io.github.lyu929.ossync.process.ProcessSimulation;
import io.github.lyu929.ossync.process.ProcessSpec;
import io.github.lyu929.ossync.process.WorkloadReader;
import io.github.lyu929.ossync.rw.ReadersWriters;
import io.github.lyu929.ossync.rw.RwLock;
import io.github.lyu929.ossync.trace.EventLog;
import java.nio.file.Path;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Command-line entry point: {@code java -jar os-sync.jar <command> [--option value ...]}. */
public final class Main {

    private static final String USAGE = """
            usage: os-sync <command> [options]

            commands:
              process    thread-per-process simulation
                         --file processes.txt  --unit-ms 100  --cpus 0 (0 = unlimited, 1 = FCFS on one CPU)
              prodcons   bounded-buffer producer/consumer
                         --impl semaphore|monitor|lock  --capacity 3  --producers 1  --consumers 2
                         --items 10 (per producer)  --produce-ms 50  --consume-ms 80  --seed 1
              rw         readers-writers
                         --policy reader|writer|fair  --readers 5  --writers 2  --ops 10
                         --read-ms 20  --write-ms 20  --think-ms 10
              dining     dining philosophers
                         --strategy naive|ordered|waiter|try-lock  --n 5  --meals 3
                         --eat-ms 20  --think-ms 20  --timeout-ms 5000  --force-deadlock
              all        run every demo with its defaults
            common: --quiet (summary only)
            """;

    private Main() {}

    public static void main(String[] args) throws Exception {
        if (args.length == 0 || args[0].equals("-h") || args[0].equals("--help")) {
            System.out.print(USAGE);
            return;
        }
        try {
            Options o = Options.parse(Arrays.copyOfRange(args, 1, args.length));
            EventLog log = new EventLog(o.flag("quiet") ? null : System.out);
            switch (args[0]) {
                case "process" -> process(o, log);
                case "prodcons" -> prodcons(o, log);
                case "rw" -> rw(o, log);
                case "dining" -> dining(o, log);
                case "all" -> {
                    process(o, log);
                    prodcons(o, log);
                    rw(o, log);
                    dining(o, log);
                }
                default -> throw new IllegalArgumentException("unknown command '" + args[0] + "'\n" + USAGE);
            }
        } catch (IllegalArgumentException e) {
            System.err.println("os-sync: error: " + e.getMessage());
            System.exit(2);
        }
    }

    private static void banner(String title) {
        System.out.println("====================================");
        System.out.println(title);
        System.out.println("====================================");
    }

    private static void process(Options o, EventLog log) throws Exception {
        List<ProcessSpec> work = WorkloadReader.read(Path.of(o.get("file", "processes.txt")));
        int cpus = o.integer("cpus", 0);
        banner("Process simulation (" + (cpus == 0 ? "unlimited CPUs" : cpus + " CPU(s)") + ")");
        ProcessSimulation.Result r = ProcessSimulation.run(work, o.integer("unit-ms", 100), cpus, log);
        System.out.println("PID  arrival  start  finish  wait   (ms)");
        for (ProcessSimulation.Run run : r.runs()) {
            System.out.printf("%3d  %7d  %5d  %6d  %4d%n", run.pid(), run.arrivalMillis(), run.startMillis(),
                    run.finishMillis(), run.waitingMillis());
        }
        System.out.println("start order " + r.startOrder() + ", elapsed " + r.elapsedMillis() + " ms\n");
    }

    private static void prodcons(Options o, EventLog log) throws Exception {
        BufferKind kind = BufferKind.parse(o.get("impl", "semaphore"));
        ProducerConsumer.Config d = ProducerConsumer.Config.course(kind);
        ProducerConsumer.Config c = new ProducerConsumer.Config(kind, o.integer("capacity", d.capacity()),
                o.integer("producers", d.producers()), o.integer("consumers", d.consumers()),
                o.integer("items", d.itemsPerProducer()), o.integer("produce-ms", (int) d.produceMillis()),
                o.integer("consume-ms", (int) d.consumeMillis()), o.integer("seed", 1), 60_000);
        banner("Producer-consumer (" + kind.name().toLowerCase() + " buffer, capacity " + c.capacity() + ")");
        ProducerConsumer.Report r = ProducerConsumer.run(c, log);
        System.out.printf("produced %d, consumed %d, max occupancy %d/%d, producer waits %d, consumer waits %d,"
                        + " %d ms, completed=%s%n%n", r.produced().size(), r.consumed().size(), r.stats().maxOccupancy(),
                c.capacity(), r.stats().fullWaits(), r.stats().emptyWaits(), r.elapsedMillis(), r.completed());
    }

    private static void rw(Options o, EventLog log) throws Exception {
        RwLock.Policy policy = RwLock.Policy.parse(o.get("policy", "fair"));
        ReadersWriters.Config c = new ReadersWriters.Config(policy, o.integer("readers", 5), o.integer("writers", 2),
                o.integer("ops", 10), o.integer("read-ms", 20), o.integer("write-ms", 20), o.integer("think-ms", 10),
                o.integer("seed", 1));
        banner("Readers-writers (" + policy.create().name() + ")");
        ReadersWriters.Report r = ReadersWriters.run(c, log);
        System.out.printf("reads %d, writes %d, violations %d, max concurrent readers %d,"
                        + " max writer wait %d ms (mean %.1f), max reader wait %d ms, %d ms%n%n", r.reads(), r.writes(),
                r.violations(), r.maxConcurrentReaders(), r.maxWriterWaitMillis(), r.meanWriterWaitMillis(),
                r.maxReaderWaitMillis(), r.elapsedMillis());
    }

    private static void dining(Options o, EventLog log) throws Exception {
        DiningPhilosophers.Strategy s = DiningPhilosophers.Strategy.parse(o.get("strategy", "ordered"));
        DiningPhilosophers.Config c = new DiningPhilosophers.Config(o.integer("n", 5), o.integer("meals", 3),
                o.integer("eat-ms", 20), o.integer("think-ms", 20), s, o.integer("timeout-ms", 5000),
                o.flag("force-deadlock"), o.integer("seed", 1));
        banner("Dining philosophers (" + s.name().toLowerCase().replace('_', '-') + ")");
        DiningPhilosophers.Report r = DiningPhilosophers.run(c, log);
        System.out.printf("meals %s, deadlocked=%s, neighbour violations %d, max eating at once %d, %d ms%n%n",
                Arrays.toString(r.meals()), r.deadlocked(), r.neighbourViolations(), r.maxEatingAtOnce(),
                r.elapsedMillis());
    }

    /** Minimal "--key value" / "--flag" parser. */
    private record Options(Map<String, String> values) {
        static Options parse(String[] args) {
            Map<String, String> m = new HashMap<>();
            for (int i = 0; i < args.length; i++) {
                if (!args[i].startsWith("--")) {
                    throw new IllegalArgumentException("unexpected argument '" + args[i] + "'");
                }
                String key = args[i].substring(2);
                if (i + 1 < args.length && !args[i + 1].startsWith("--")) {
                    m.put(key, args[++i]);
                } else {
                    m.put(key, "true");
                }
            }
            return new Options(m);
        }

        String get(String key, String dflt) {
            return values.getOrDefault(key, dflt);
        }

        int integer(String key, int dflt) {
            String v = values.get(key);
            if (v == null) {
                return dflt;
            }
            try {
                return Integer.parseInt(v);
            } catch (NumberFormatException e) {
                throw new IllegalArgumentException("--" + key + " expects an integer, got '" + v + "'", e);
            }
        }

        boolean flag(String key) {
            return Boolean.parseBoolean(values.getOrDefault(key, "false"));
        }
    }
}
