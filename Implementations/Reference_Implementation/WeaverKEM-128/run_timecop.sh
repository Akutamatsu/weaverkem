#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../../../.." && pwd)"
SRC="$ROOT/weaverkem/ref/src"
cd "$HERE"

gcc -O0 -g -Wall -Wextra \
  -I"$HERE" -I"$SRC" \
  -o timecop_weaver128 \
  timecop_harness.c \
  drng.c auxfunc.c KEM_WeaverKEM-128.c \
  "$SRC/kem_cca.c" \
  "$SRC/indcpa.c" \
  "$SRC/polyvec.c" \
  "$SRC/poly.c" \
  "$SRC/ntt.c" \
  "$SRC/cbd.c" \
  "$SRC/reduce.c" \
  "$SRC/verify.c" \
  "$SRC/fips202.c" \
  "$SRC/symmetric-shake.c" \
  "$SRC/msgenc.c" \
  "$SRC/aes256ctr.c" \
  "$SRC/bch_high.c" \
  "$SRC/bch_low.c"

valgrind --track-origins=yes --error-limit=no \
  ./timecop_weaver128 2>&1 | tee timecop_valgrind.log

echo "Wrote $HERE/timecop_valgrind.log"
