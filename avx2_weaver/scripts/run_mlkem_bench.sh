#!/bin/bash
# ML-KEM (pq-crystals/kyber) speed benchmarks for Section 5.
set -euo pipefail
KYBER_DIR="${KYBER_DIR:-/tmp/mlkem-ref}"
OUT="${OUT:-/root/weaverkem/avx2_weaver/bench_section5/mlkem}"
NRUNS="${NRUNS:-5}"
mkdir -p "$OUT"

cd "$KYBER_DIR"
COMMIT=$(git rev-parse --short HEAD)
echo "mlkem commit: $COMMIT" | tee "$OUT/commit.txt"

run_speed() {
  local subdir=$1 variant=$2
  local bin="test/test_speed${variant}"
  local tag="mlkem${variant}_${subdir}"
  local out="$OUT/${tag}.txt"
  : > "$out"
  (
    cd "$KYBER_DIR/$subdir"
    for i in $(seq 1 "$NRUNS"); do
      echo "=== run $i ==="
      "./${bin}"
    done
  ) >> "$out" 2>&1
  echo "Wrote $out"
}

for variant in 512 768 1024; do
  echo "=== AVX2 Kyber-${variant} ==="
  make -C "$KYBER_DIR/avx2" clean >/dev/null 2>&1 || true
  make -C "$KYBER_DIR/avx2" "test/test_speed${variant}" 2>&1 | tail -2
  run_speed avx2 "$variant"

  echo "=== REF Kyber-${variant} ==="
  make -C "$KYBER_DIR/ref" clean >/dev/null 2>&1 || true
  make -C "$KYBER_DIR/ref" "test/test_speed${variant}" 2>&1 | tail -2
  run_speed ref "$variant"
done

echo "Done ML-KEM benches in $OUT"
