#!/usr/bin/env bash
# Compare scalar vs AVX128 gen_matrix outputs (mode 1).
set -euo pipefail
cd "$(dirname "$0")/.."
NCASES="${1:-50}"
bin/test_gen_matrix1_scalar "$NCASES" > /tmp/gm1_scalar.bin
bin/test_gen_matrix1_avx "$NCASES" > /tmp/gm1_avx.bin
if cmp -s /tmp/gm1_scalar.bin /tmp/gm1_avx.bin; then
  echo "PASS: gen_matrix mode1 scalar == AVX128 ($NCASES cases)"
  exit 0
fi
echo "FAIL: gen_matrix mode1 mismatch"
cmp -l /tmp/gm1_scalar.bin /tmp/gm1_avx.bin | head -20
exit 1
