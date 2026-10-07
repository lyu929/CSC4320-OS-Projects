package io.github.lyu929.ossync.dining;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import io.github.lyu929.ossync.dining.DiningPhilosophers.Strategy;
import io.github.lyu929.ossync.trace.EventLog;
import java.util.concurrent.TimeUnit;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.Timeout;

@Timeout(value = 60, unit = TimeUnit.SECONDS)
class DiningPhilosophersTest {

    @Test
    void naiveStrategyDeadlocksWhenEveryoneHoldsTheLeftFork() throws Exception {
        DiningPhilosophers.Config c = new DiningPhilosophers.Config(5, 3, 1, 1, Strategy.NAIVE, 500, true, 1);
        EventLog log = new EventLog();
        DiningPhilosophers.Report r = DiningPhilosophers.run(c, log);
        assertTrue(r.deadlocked());
        assertEquals(0, r.totalMeals());
        assertTrue(log.events().stream().anyMatch(e -> e.actor().equals("Watchdog")));
    }

    @Test
    void deadlockFreeStrategiesFeedEveryone() throws Exception {
        for (Strategy s : new Strategy[] {Strategy.ORDERED, Strategy.WAITER, Strategy.TRY_LOCK}) {
            DiningPhilosophers.Config c = new DiningPhilosophers.Config(5, 25, 1, 1, s, 20_000, false, 9);
            DiningPhilosophers.Report r = DiningPhilosophers.run(c, new EventLog());
            assertFalse(r.deadlocked(), s.name());
            for (int m : r.meals()) {
                assertEquals(25, m, s + " starved someone");
            }
            assertEquals(0, r.neighbourViolations(), s + " let neighbours share a fork");
            assertTrue(r.maxEatingAtOnce() <= 2, s + ": at most floor(5/2) can eat at once");
        }
    }

    @Test
    void configIsValidated() {
        assertThrows(IllegalArgumentException.class,
                () -> new DiningPhilosophers.Config(1, 1, 1, 1, Strategy.ORDERED, 100, false, 0));
        assertThrows(IllegalArgumentException.class,
                () -> new DiningPhilosophers.Config(5, 1, 1, 1, Strategy.ORDERED, 100, true, 0));
        assertEquals(Strategy.TRY_LOCK, Strategy.parse("try-lock"));
    }
}
