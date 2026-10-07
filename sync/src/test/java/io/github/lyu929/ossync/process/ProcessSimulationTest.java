package io.github.lyu929.ossync.process;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import io.github.lyu929.ossync.trace.EventLog;
import java.util.List;
import java.util.concurrent.TimeUnit;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.Timeout;

@Timeout(value = 60, unit = TimeUnit.SECONDS)
class ProcessSimulationTest {

    private static final String COURSE = """
            PID Arrival Burst Priority
            1 0 7 2
            2 1 4 1
            3 3 6 3
            4 5 3 2
            5 7 5 1
            """;

    @Test
    void oneCpuReproducesFcfsOrder() throws Exception {
        List<ProcessSpec> work = WorkloadReader.parse(COURSE);
        long unit = 20;
        ProcessSimulation.Result r = ProcessSimulation.run(work, unit, 1, new EventLog());
        assertEquals(List.of(1, 2, 3, 4, 5), r.startOrder());
        // a single CPU can never finish 25 units of work faster than 25 units
        assertTrue(r.elapsedMillis() >= 25 * unit, "elapsed " + r.elapsedMillis());
        for (ProcessSimulation.Run run : r.runs()) {
            assertTrue(run.finishMillis() - run.startMillis() >= burst(work, run.pid()) * unit);
        }
    }

    @Test
    void unlimitedCpusRunEveryProcessOnArrival() throws Exception {
        List<ProcessSpec> work = WorkloadReader.parse(COURSE);
        ProcessSimulation.Result r = ProcessSimulation.run(work, 20, 0, new EventLog());
        assertEquals(5, r.runs().size());
        for (ProcessSimulation.Run run : r.runs()) {
            assertTrue(run.waitingMillis() < 15, "P" + run.pid() + " waited " + run.waitingMillis() + " ms");
        }
        assertTrue(r.elapsedMillis() < 25 * 20, "processes should overlap");
    }

    @Test
    void readerAcceptsCommentsCommasAndOptionalPriority() {
        List<ProcessSpec> w = WorkloadReader.parse("# demo\nPID A B\n1, 0, 5\n2 3 4 1 # trailing\n");
        assertEquals(List.of(new ProcessSpec(1, 0, 5, 0), new ProcessSpec(2, 3, 4, 1)), w);
    }

    @Test
    void readerReportsLineNumbers() {
        IllegalArgumentException e = assertThrows(IllegalArgumentException.class,
                () -> WorkloadReader.parse("1 0 5 1\n2 x 3 1\n"));
        assertTrue(e.getMessage().contains("<input>:2: arrival is not an integer"), e.getMessage());
        e = assertThrows(IllegalArgumentException.class, () -> WorkloadReader.parse("1 0 5 1\n\n1 2 3 1\n"));
        assertTrue(e.getMessage().contains(":3: duplicate PID 1 (first on line 1)"), e.getMessage());
        e = assertThrows(IllegalArgumentException.class, () -> WorkloadReader.parse("1 0 0 1\n"));
        assertTrue(e.getMessage().contains("burst must be > 0"), e.getMessage());
        assertThrows(IllegalArgumentException.class, () -> WorkloadReader.parse("PID A B\n"));
    }

    @Test
    void invalidArgumentsAreRejected() {
        List<ProcessSpec> w = WorkloadReader.parse(COURSE);
        assertThrows(IllegalArgumentException.class, () -> ProcessSimulation.run(w, 0, 1, new EventLog()));
        assertThrows(IllegalArgumentException.class, () -> ProcessSimulation.run(w, 10, -1, new EventLog()));
    }

    private static int burst(List<ProcessSpec> work, int pid) {
        return work.stream().filter(p -> p.pid() == pid).findFirst().orElseThrow().burst();
    }
}
