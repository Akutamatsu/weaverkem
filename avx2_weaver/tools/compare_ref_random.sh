#!/bin/sh
# Compare ref vs AVX KEM derand replay byte streams.
set -e
REF_BIN="$1"
AVX_BIN="$2"
NCASES="${3:-10000}"
SEED="${4:-C0FFEE42}"
TMP="${TMPDIR:-/tmp}/weaver_cmp_$$"
trap 'rm -f "$TMP.ref" "$TMP.avx"' EXIT

"$REF_BIN" "$NCASES" "$SEED" > "$TMP.ref"
"$AVX_BIN" "$NCASES" "$SEED" > "$TMP.avx"

if cmp -s "$TMP.ref" "$TMP.avx"; then
  echo "PASS: $NCASES random cases — ref and avx2 byte-identical (KEM derand path)."
  exit 0
fi
echo "FAIL: ref vs avx2 replay mismatch ($NCASES cases, seed=$SEED)."
exit 1
