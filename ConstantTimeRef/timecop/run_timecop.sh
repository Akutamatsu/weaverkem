#!/usr/bin/env bash
# Run under Linux or WSL where valgrind is installed.
# Windows MSYS2 ucrt64 通常无 valgrind，请用:  wsl -e bash -lc 'cd /mnt/c/.../weaverkem/ref/timecop && ./run_timecop.sh'
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$HERE/../src"
cd "$HERE"

# 与 ConstantTimeRef/timecop/run_timecop.sh 相比：源码路径为 ref/src，并链接 poly_invq.c（ref 的 kem_cca 依赖 invq）；无 aes256ctr（ref 仅用 SHAKE 对称层）。
KEM_SRC=(
  "$SRC/kem_cca.c"
  "$SRC/indcpa.c"
  "$SRC/polyvec.c"
  "$SRC/poly.c"
  "$SRC/ntt.c"
  "$SRC/cbd.c"
  "$SRC/reduce.c"
  "$SRC/verify.c"
  "$SRC/fips202.c"
  "$SRC/symmetric-shake.c"
  "$SRC/msgenc.c"
  "$SRC/poly_invq.c"
  "$SRC/bch_high.c"
  "$SRC/bch_low.c"
  "$SRC/rng.c"
)

MODES="${WEAVER_MODES:-1 3 5}"

for m in ${MODES}; do
  case "${m}" in
    1) LABEL="512" ;;
    3) LABEL="1024" ;;
    5) LABEL="2048" ;;
    *) LABEL="mode${m}" ;;
  esac
  BIN="timecop_ref_weaver_${LABEL}"
  LOG="timecop_valgrind_WEAVER-${LABEL}.log"

  echo "=== Compiling ref TIMECOP harness WEAVER_MODE=${m} (${LABEL}) -> ${BIN} ==="
  gcc -O0 -g -Wall -Wextra \
    -I"${SRC}" \
    -DWEAVER_MODE="${m}" \
    -o "${BIN}" \
    timecop_harness.c \
    "${KEM_SRC[@]}" \
    -lcrypto

  echo "=== Valgrind ${BIN} -> ${LOG} ==="
  valgrind --track-origins=yes --error-limit=no \
    "./${BIN}" 2>&1 | tee "${LOG}"
done

echo "Done. Logs in: ${HERE}/timecop_valgrind_WEAVER-*.log"
