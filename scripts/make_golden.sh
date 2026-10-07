#!/usr/bin/env bash
# Regenerate tests/golden/ with the C++ simulator. The JavaScript engine (web/) is
# checked against these files by `node --test web/test`; CI regenerates them and fails
# if they differ from the committed copies (C++ determinism across compilers).
#
#   scripts/make_golden.sh [path/to/schedsim] [output dir]
set -euo pipefail
SIM=${1:-build/dev/scheduler/schedsim}
OUT=${2:-tests/golden}
mkdir -p "$OUT/workloads"
dists=(exp uniform bimodal)

cases=()
cp scheduler/data/processes.txt "$OUT/workloads/course.txt"
cases+=("course||")
for seed in 1 2 3 4 5 6 7 8 9; do
  dist=${dists[$((seed % 3))]}
  load=$(awk "BEGIN{print 0.6 + 0.1 * ($seed % 5)}")
  "$SIM" gen -n 40 --seed "$seed" --load "$load" --dist "$dist" --mean 6 > "$OUT/workloads/w$seed.txt"
  cases+=("w$seed|$seed|$dist|$load")
done

manifest="$OUT/manifest.json"
echo "[" > "$manifest"
first=1
for c in "${cases[@]}"; do
  IFS='|' read -r name seed dist load <<< "$c"
  w="$OUT/workloads/$name.txt"
  gen="null"
  if [[ -n "$seed" ]]; then
    gen="{\"count\": 40, \"seed\": $seed, \"dist\": \"$dist\", \"load\": $load, \"meanBurst\": 6}"
  fi
  # two parameter sets per workload: tuned policies, and context-switch overhead
  "$SIM" -a all -q 3 --mlfq 2/4/8 --boost 30 --aging 5 --format json "$w" > "$OUT/$name.json"
  "$SIM" -a all -q 2 --mlfq 1/2 --cs 1 --format json "$w" > "$OUT/$name-cs.json"
  for variant in "$name.json|{\"quantum\": 3, \"mlfqQuanta\": [2, 4, 8], \"mlfqBoost\": 30, \"aging\": 5, \"contextSwitch\": 0}" \
                 "$name-cs.json|{\"quantum\": 2, \"mlfqQuanta\": [1, 2], \"mlfqBoost\": 0, \"aging\": 0, \"contextSwitch\": 1}"; do
    IFS='|' read -r file params <<< "$variant"
    [[ $first -eq 1 ]] || echo "," >> "$manifest"
    first=0
    printf '  {"workload": "workloads/%s.txt", "expected": "%s", "params": %s, "generator": %s}' \
      "$name" "$file" "$params" "$gen" >> "$manifest"
  done
done
printf '\n]\n' >> "$manifest"
echo "wrote $(ls "$OUT"/*.json | wc -l) files to $OUT"
