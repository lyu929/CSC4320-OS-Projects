package io.github.lyu929.ossync.buffer;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import io.github.lyu929.ossync.trace.EventLog;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.concurrent.TimeUnit;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.Timeout;

@Timeout(value = 60, unit = TimeUnit.SECONDS)
class ProducerConsumerTest {

    private static final Comparator<ProducerConsumer.Item> ORDER =
            Comparator.comparingInt(ProducerConsumer.Item::producer).thenComparingInt(ProducerConsumer.Item::seq);

    @Test
    void courseScenarioCompletesWithEveryImplementation() throws Exception {
        for (BufferKind kind : BufferKind.values()) {
            ProducerConsumer.Config base = ProducerConsumer.Config.course(kind);
            ProducerConsumer.Config fast = new ProducerConsumer.Config(kind, base.capacity(), base.producers(),
                    base.consumers(), base.itemsPerProducer(), 5, 8, 1, 20_000);
            EventLog log = new EventLog();
            ProducerConsumer.Report r = ProducerConsumer.run(fast, log);
            assertTrue(r.completed(), kind.name());
            assertEquals(10, r.consumed().size());
            assertEquals(sorted(r.produced()), sorted(r.consumed()), kind + ": consumed != produced");
            assertTrue(r.stats().maxOccupancy() <= 3);
            assertTrue(log.events().stream().anyMatch(e -> e.message().startsWith("consumed item 1-1")));
        }
    }

    @Test
    void unevenSplitsTerminateThanksToPoisonPills() throws Exception {
        // 3 x 7 = 21 items over 4 consumers: the course design (fixed items per consumer) would hang here.
        ProducerConsumer.Config c = new ProducerConsumer.Config(BufferKind.LOCK, 2, 3, 4, 7, 1, 2, 7, 20_000);
        ProducerConsumer.Report r = ProducerConsumer.run(c, new EventLog());
        assertTrue(r.completed());
        assertEquals(21, r.consumed().size());
        assertEquals(21, r.perConsumer().stream().mapToInt(List::size).sum());
    }

    @Test
    void configIsValidated() {
        assertThrows(IllegalArgumentException.class,
                () -> new ProducerConsumer.Config(BufferKind.LOCK, 0, 1, 1, 1, 0, 0, 0, 0));
        assertThrows(IllegalArgumentException.class,
                () -> new ProducerConsumer.Config(BufferKind.LOCK, 1, 1, 1, 1, -1, 0, 0, 0));
    }

    private static List<ProducerConsumer.Item> sorted(List<ProducerConsumer.Item> items) {
        List<ProducerConsumer.Item> copy = new ArrayList<>(items);
        copy.sort(ORDER);
        return copy;
    }
}
