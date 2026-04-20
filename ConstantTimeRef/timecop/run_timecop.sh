#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$HERE/../src"

cd "$HERE"

echo "Compiling ConstantTimeRef TIMECOP harness..."
gcc -O0 -g -Wall -Wextra \
  -I"$SRC" \
  -DWEAVER_MODE=1 \
  -o timecop_ct_weaver \
  timecop_harness.c \
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
  "$SRC/bch_low.c" \
  "$SRC/rng.c" \
  -lcrypto

echo "Running Valgrind/TIMECOP scan..."
valgrind --track-origins=yes --error-limit=no \
  ./timecop_ct_weaver 2>&1 | tee timecop_valgrind.log

echo "Results written to $HERE/timecop_valgrind.log"
