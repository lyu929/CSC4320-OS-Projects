package io.github.lyu929.ossync.process;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.Reader;
import java.io.StringReader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Parses the shared workload format ("PID Arrival Burst [Priority]" per line, optional header,
 * {@code #} comments, whitespace or commas) used by the C++ scheduler as well.
 */
public final class WorkloadReader {

    private WorkloadReader() {}

    public static List<ProcessSpec> read(Path path) throws IOException {
        try (Reader r = Files.newBufferedReader(path)) {
            return parse(r, path.toString());
        }
    }

    public static List<ProcessSpec> parse(String text) {
        try {
            return parse(new StringReader(text), "<input>");
        } catch (IOException e) {
            throw new IllegalStateException(e); // cannot happen for a StringReader
        }
    }

    public static List<ProcessSpec> parse(Reader reader, String source) throws IOException {
        BufferedReader in = new BufferedReader(reader);
        List<ProcessSpec> out = new ArrayList<>();
        Map<Integer, Integer> firstLine = new HashMap<>();
        boolean seenContent = false;
        int lineNo = 0;
        for (String line = in.readLine(); line != null; line = in.readLine()) {
            lineNo++;
            int hash = line.indexOf('#');
            if (hash >= 0) {
                line = line.substring(0, hash);
            }
            String trimmed = line.replace(',', ' ').trim();
            if (trimmed.isEmpty()) {
                continue;
            }
            String[] f = trimmed.split("\\s+");
            if (!seenContent && !isInt(f[0])) { // header row
                seenContent = true;
                continue;
            }
            seenContent = true;
            String where = source + ":" + lineNo + ": ";
            if (f.length < 3 || f.length > 4) {
                throw new IllegalArgumentException(where + "expected 'PID Arrival Burst [Priority]', got "
                        + f.length + " fields");
            }
            int[] v = new int[4];
            String[] names = {"PID", "arrival", "burst", "priority"};
            for (int i = 0; i < f.length; i++) {
                if (!isInt(f[i])) {
                    throw new IllegalArgumentException(where + names[i] + " is not an integer: '" + f[i] + "'");
                }
                v[i] = Integer.parseInt(f[i]);
            }
            Integer previous = firstLine.putIfAbsent(v[0], lineNo);
            if (previous != null) {
                throw new IllegalArgumentException(where + "duplicate PID " + v[0] + " (first on line " + previous + ")");
            }
            try {
                out.add(new ProcessSpec(v[0], v[1], v[2], v[3]));
            } catch (IllegalArgumentException e) {
                throw new IllegalArgumentException(where + e.getMessage(), e);
            }
        }
        if (out.isEmpty()) {
            throw new IllegalArgumentException(source + ": no processes found");
        }
        return out;
    }

    private static boolean isInt(String s) {
        return s.matches("[+-]?\\d{1,9}");
    }
}
