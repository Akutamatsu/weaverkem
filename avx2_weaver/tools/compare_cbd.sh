#!/usr/bin/env bash
# Compare AVX2 CBD vs scalar ref on random PRF buffers.
set -euo pipefail
cd "$(dirname "$0")/.."
MODE="${1:-3}"
NCASES="${2:-50}"

case "$MODE" in
  1) OUT=/tmp/cbd1.bin; BD=cbd1; M=1; TAG=640 ;;
  3) OUT=/tmp/cbd3.bin; BD=cbd3; M=3; TAG=1024 ;;
  5) OUT=/tmp/cbd5.bin; BD=cbd5; M=5; TAG=2048 ;;
  *) echo "usage: $0 {1|3|5} [ncases]" >&2; exit 1 ;;
esac

cat > tools/dump_cbd.c <<'EOF'
#include <stdio.h>
#include <stdint.h>
#include "params.h"
#include "poly.h"
#include "symmetric.h"

static void write_poly(FILE *out, const poly *p)
{
  unsigned i;
  for(i = 0; i < WEAVER_N; i++)
    fwrite(&p->coeffs[i], sizeof(int16_t), 1, out);
}

int main(void)
{
  uint8_t buf[WEAVER_ETA1 * WEAVER_N / 4];
  poly r;
  unsigned t, i;

  for(t = 0; t < 50; t++) {
    for(i = 0; i < sizeof(buf); i++)
      buf[i] = (uint8_t)(0x37 ^ (t * 19 + i * 7));
    cbd_eta1(&r, buf);
    if(fwrite(buf, 1, sizeof(buf), stdout) != sizeof(buf)) return 1;
    write_poly(stdout, &r);
  }
  return 0;
}
EOF

rm -rf "build/${BD}s" "build/${BD}a"
USE_AVX_CBD=0 make -s "bin/dump_cbd_${TAG}_scalar" 2>/dev/null || {
  mkdir -p bin "build/${BD}s"
  for f in src/cbd.c src/fips202.c src/symmetric-shake.c; do
    gcc -std=c99 -O2 -DWEAVER_MODE=$M -Isrc -c "$f" -o "build/${BD}s/$(basename "$f" .c).o"
  done
  gcc -std=c99 -O2 -DWEAVER_MODE=$M -Isrc -c tools/dump_cbd.c -o "build/${BD}s/dump_cbd.o"
  gcc -o "bin/dump_cbd_${TAG}_scalar" "build/${BD}s"/*.o
}

USE_AVX_CBD=1 make -s "bin/dump_cbd_${TAG}_avx" 2>/dev/null || {
  mkdir -p bin "build/${BD}a"
  for f in src/cbd.c src/cbd_avx2.c src/fips202.c src/symmetric-shake.c; do
    cflags="-std=c99 -O2 -mavx2 -mbmi2 -DWEAVER_MODE=$M -Isrc -DWEAVER_USE_AVX_CBD"
    gcc $cflags -c "$f" -o "build/${BD}a/$(basename "$f" .c).o"
  done
  gcc $cflags -c tools/dump_cbd.c -o "build/${BD}a/dump_cbd.o"
  gcc -o "bin/dump_cbd_${TAG}_avx" "build/${BD}a"/*.o
}

bin/dump_cbd_${TAG}_scalar "$NCASES" > /tmp/cbd_scalar.bin
bin/dump_cbd_${TAG}_avx "$NCASES" > /tmp/cbd_avx.bin
if cmp -s /tmp/cbd_scalar.bin /tmp/cbd_avx.bin; then
  echo "PASS: CBD mode$MODE scalar == AVX ($NCASES cases)"
else
  echo "FAIL: CBD mode$MODE mismatch"
  cmp -l /tmp/cbd_scalar.bin /tmp/cbd_avx.bin | head -20
  exit 1
fi
