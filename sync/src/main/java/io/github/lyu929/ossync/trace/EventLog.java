package io.github.lyu929.ossync.trace;

import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.atomic.AtomicLong;

/**
 * Thread-safe, totally ordered event trace.
 *
 * <p>Every event gets a global sequence number and a timestamp relative to the creation of the log,
 * so traces from concurrent threads can be read (and asserted on) in a single, consistent order.
 * Optionally echoes each event to a stream as it happens, like the original course output.
 */
public final class EventLog {

    /** One trace entry. */
    public record Event(long seq, long micros, String actor, String message) {
        @Override
        public String toString() {
            return String.format("%6.1f ms  [%s] %s", micros / 1000.0, actor, message);
        }
    }

    private final long start = System.nanoTime();
    private final AtomicLong seq = new AtomicLong();
    private final List<Event> events = Collections.synchronizedList(new ArrayList<>());
    private final PrintStream echo;

    /** A silent log. */
    public EventLog() {
        this(null);
    }

    /** A log that also prints every event to {@code echo} (may be null). */
    public EventLog(PrintStream echo) {
        this.echo = echo;
    }

    /** Record an event. Safe to call from any thread. */
    public void log(String actor, String message) {
        Event e = new Event(seq.getAndIncrement(), (System.nanoTime() - start) / 1000, actor, message);
        events.add(e);
        if (echo != null) {
            synchronized (echo) {
                echo.println(e);
            }
        }
    }

    /** Snapshot of all events in sequence order. */
    public List<Event> events() {
        synchronized (events) {
            List<Event> copy = new ArrayList<>(events);
            copy.sort((a, b) -> Long.compare(a.seq(), b.seq()));
            return copy;
        }
    }

    public int size() {
        return events.size();
    }
}
