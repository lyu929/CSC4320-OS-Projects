package io.github.lyu929.ossync.rw;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;

import io.github.lyu929.ossync.trace.EventLog;
import java.util.concurrent.TimeUnit;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.Timeout;

@Timeout(value = 90, unit = TimeUnit.SECONDS)
class ReadersWritersTest {

    @Test
    void everyPolicyEnforcesExclusion() throws Exception {
        for (RwLock.Policy policy : RwLock.Policy.values()) {
            ReadersWriters.Config c = new ReadersWriters.Config(policy, 6, 3, 40, 1, 1, 1, 3);
            ReadersWriters.Report r = ReadersWriters.run(c, new EventLog());
            assertEquals(0, r.violations(), policy + " let a writer overlap another thread");
            assertEquals(240, r.reads());
            assertEquals(120, r.writes());
            assertTrue(r.maxConcurrentReaders() > 1, policy + " never let readers share the lock");
        }
    }

    @Test
    void readerPreferenceStarvesWritersAndWriterPreferenceDoesNot() throws Exception {
        // Eight readers keep the lock continuously busy; one writer wants in.
        ReadersWriters.Report readerPref = ReadersWriters.run(
                new ReadersWriters.Config(RwLock.Policy.READER_PREFERENCE, 8, 1, 12, 30, 5, 0, 5), new EventLog());
        ReadersWriters.Report writerPref = ReadersWriters.run(
                new ReadersWriters.Config(RwLock.Policy.WRITER_PREFERENCE, 8, 1, 12, 30, 5, 0, 5), new EventLog());
        ReadersWriters.Report fair = ReadersWriters.run(
                new ReadersWriters.Config(RwLock.Policy.FAIR, 8, 1, 12, 30, 5, 0, 5), new EventLog());
        assertTrue(readerPref.maxWriterWaitMillis() > 150,
                "reader preference should make the writer wait, got " + readerPref.maxWriterWaitMillis());
        assertTrue(readerPref.maxWriterWaitMillis() > 3 * writerPref.maxWriterWaitMillis(),
                readerPref.maxWriterWaitMillis() + " vs " + writerPref.maxWriterWaitMillis());
        assertTrue(readerPref.maxWriterWaitMillis() > 3 * fair.maxWriterWaitMillis(),
                readerPref.maxWriterWaitMillis() + " vs fair " + fair.maxWriterWaitMillis());
    }

    @Test
    void policyNamesParse() {
        assertEquals(RwLock.Policy.FAIR, RwLock.Policy.parse("FIFO"));
        assertEquals(RwLock.Policy.WRITER_PREFERENCE, RwLock.Policy.parse("writer"));
        assertThrows(IllegalArgumentException.class, () -> RwLock.Policy.parse("random"));
        assertThrows(IllegalArgumentException.class,
                () -> new ReadersWriters.Config(RwLock.Policy.FAIR, 0, 0, 1, 0, 0, 0, 0));
    }
}
