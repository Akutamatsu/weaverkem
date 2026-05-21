#!/bin/bash
# Section 5 full benchmark harness for Weaver KEM (modes 1, 3, 5).
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=bench_section5
mkdir -p "$OUT"
NRUNS="${NRUNS:-5}"

run_weaver_bench() {
  local mode=$1 tag=$2 binary=$3
  local out="$OUT/${tag}.txt"
  : > "$out"
  for i in $(seq 1 "$NRUNS"); do
    echo "=== run $i $(date -Iseconds) ===" >> "$out"
    "./bin/bench_components_${binary}" 2>&1 >> "$out"
  done
  echo "Wrote $out"
}

build_avx() {
  make USE_AVX_NTT=1 USE_AVX_COMPRESS=1 \
    USE_AVX_FQ_512=1 USE_AVX_GEN_MATRIX512=1 \
    bin/bench_components_512 bin/bench_components_768 bin/bench_components_1024
}

build_ref() {
  make USE_AVX_NTT=0 USE_AVX_COMPRESS=0 \
    USE_AVX_FQ_512=0 USE_AVX_GEN_MATRIX512=0 \
    bin/bench_components_512 bin/bench_components_768 bin/bench_components_1024
}

echo "=== Environment ===" | tee "$OUT/environment.txt"
{
  cat /proc/cpuinfo | grep "model name" | head -1
  gcc --version | head -1
  uname -r
} | tee -a "$OUT/environment.txt"

echo "=== Build AVX ==="
make clean >/dev/null
build_avx
run_weaver_bench 1 w512_avx 512
run_weaver_bench 3 w768_avx 768
run_weaver_bench 5 w1024_avx 1024

echo "=== Build REF ==="
make clean >/dev/null
build_ref
run_weaver_bench 1 w512_ref 512
run_weaver_bench 3 w768_ref 768
run_weaver_bench 5 w1024_ref 1024

echo "Done. Results in $OUT/"
