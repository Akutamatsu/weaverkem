#!/bin/sh
set -e
REF_BIN="$1"
AVX_BIN="$2"
REF_OUT="${3:-/tmp/trace_encap_ref.txt}"
AVX_OUT="${4:-/tmp/trace_encap_avx.txt}"

"$REF_BIN" > "$REF_OUT"
"$AVX_BIN" > "$AVX_OUT"

NORM_REF="${REF_OUT}.norm"
NORM_AVX="${AVX_OUT}.norm"
sed 's/impl=ref/impl=X/; s/impl=avx/impl=X/' "$REF_OUT" > "$NORM_REF"
sed 's/impl=ref/impl=X/; s/impl=avx/impl=X/' "$AVX_OUT" > "$NORM_AVX"

if cmp -s "$NORM_REF" "$NORM_AVX"; then
  echo "PASS: all trace labels match between ref and avx."
  exit 0
fi

FIRST=$(diff -u "$NORM_REF" "$NORM_AVX" | awk '/^[-+][^-+]/ && !/^[-+]{3}/ {sub(/^[-+]/,""); print; exit}')
echo "FAIL: first trace mismatch:"
echo "$FIRST"
diff -u "$REF_OUT" "$AVX_OUT" | head -20
exit 1
