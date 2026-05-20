#!/usr/bin/env bash
# Compare scalar vs AVX512 gen_matrix outputs (mode 5).
set -euo pipefail
cd "$(dirname "$0")/.."
NCASES="${1:-50}"
make -s bin/test_gen_matrix5_scalar bin/test_gen_matrix5_avx
bin/test_gen_matrix5_scalar "$NCASES" > /tmp/gm5_scalar.bin
bin/test_gen_matrix5_avx "$NCASES" > /tmp/gm5_avx.bin
if cmp -s /tmp/gm5_scalar.bin /tmp/gm5_avx.bin; then
  echo "PASS: gen_matrix mode5 scalar == AVX512 ($NCASES cases, A and A^T)"
  exit 0
fi
echo "FAIL: gen_matrix mode5 mismatch"
cmp -l /tmp/gm5_scalar.bin /tmp/gm5_avx.bin | head -20
exit 1
