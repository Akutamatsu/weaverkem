# WeaverKEM AVX2 Optimized Implementation

AVX2-accelerated build of WeaverKEM, based on the reference code in `ref/src/` and Kyber AVX2 primitives (`avx2/src/`).

## Layout

- `src/` — Weaver algorithm (indcpa, kem, BCH, msgenc, polyvec) + AVX2 NTT/basemul for `n=256`
- `src/avx/` — NTT / basemul / Barrett reduction assembly (from Kyber AVX2)
- `tools/` — KAT verification and extended test vectors
- `bench/` — Cycle-count benchmarks

## Build

```bash
cd avx2_weaver
make all      # binaries in bin/
make test     # ICCS KAT + ref cross-check + extended vectors
make bench    # keypair / encaps / decaps timing
```

## Parameter sets

| Binary | WEAVER_MODE | Paper name | AVX NTT |
|--------|-------------|------------|---------|
| `weaver_avx_512` | 1 | WEAVER-512 | yes (n=256) |
| `weaver_avx_1024` | 3 | WEAVER-1024 | yes |
| `weaver_avx_2048` | 5 | WEAVER-2048 | no (portable C NTT, n=512) |

## Correctness

1. **ICCS KAT** — replays `Test_Vectors/KAT_KEM_WeaverKEM-*.txt` with SM3-DRNG (`kat_iccs_256`, `kat_iccs_128`).
2. **Reference cross-check** — `gen_vectors_ref_*` builds 100 vectors from `ref/`; `compare_ref_avx_*` checks byte-identical outputs from this tree.
3. **Self-test** — `gen_vectors_*` + `verify_vectors_*` on extended seeds.

## Optimizations (n=256)

- AVX2 NTT / invNTT / basemul (assembly)
- SIMD `poly_add` / `poly_sub` / `poly_reduce`
- Portable C for Weaver-specific compression (9-bit PK, 4/5-bit `v`), BCH, Inv_q lifting
