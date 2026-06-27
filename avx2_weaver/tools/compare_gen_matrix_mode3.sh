#!/usr/bin/env bash
set -euo pipefail

NCASES="${1:-50}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

SCALAR=bin/test_gen_matrix3_scalar
AVX=bin/test_gen_matrix3_avx

"$SCALAR" "$NCASES" > /tmp/weaver_gm3_scalar.bin
"$AVX" "$NCASES" > /tmp/weaver_gm3_avx.bin

if cmp -s /tmp/weaver_gm3_scalar.bin /tmp/weaver_gm3_avx.bin; then
  echo "PASS: gen_matrix mode3 scalar vs avx ($NCASES cases)"
else
  echo "FAIL: gen_matrix mode3 mismatch"
  ls -l /tmp/weaver_gm3_scalar.bin /tmp/weaver_gm3_avx.bin
  exit 1
fi
