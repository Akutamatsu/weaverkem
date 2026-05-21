# WeaverKEM AVX2 Optimized Implementation

AVX2-oriented build of WeaverKEM, based on `ref/src/` and Kyber AVX2 primitives (`avx2/src/`). Algorithm logic matches the ICCS Reference_Implementation; `Test_Vectors/` is generated from that tree.

## Layout

- `src/` — Weaver algorithm (indcpa, kem, BCH, msgenc, polyvec) + optional AVX2 NTT/basemul for `n=256` (WIP)
- `src/avx/` — NTT / basemul / Barrett reduction assembly (from Kyber AVX2)
- `tools/` — KAT verification and extended test vectors
- `bench/` — Cycle-count benchmarks

## Build

Requires **x86_64** with AVX2 (`-mavx2 -mbmi2 -mpopcnt`). On Linux, `gcc` or `clang` is auto-selected.

```bash
cd avx2_weaver
make all      # binaries in bin/
make kat      # replay ../Test_Vectors/ (must match ref; all 3 security levels)
make test     # KAT + ref/AVX checks for WEAVER-512, 768, and 1024 (all three levels)
make test-extended  # test + mode-5 gen_matrix diff + enc_derand buffer regression
make bench    # keypair / encaps / decaps timing
```

Override the compiler if needed: `make CC=gcc all`.
Performance toggles:

- `make PERF_NATIVE=1 ENABLE_LTO=1 bench` (default): `-march=native` + LTO for host CPU.
- `make PERF_NATIVE=0 ENABLE_LTO=0 bench`: conservative portable flags.
- `make USE_AVX_NTT=1 test`: tries AVX2 NTT path for modes 1/3 (must pass vector checks before use).

## Parameter sets

| Binary | WEAVER_MODE | Parameter set | ICCS KAT file |
|--------|-------------|---------------|---------------|
| `weaver_avx_512` | 1 | WEAVER-512 | `KAT_KEM_WeaverKEM-128.txt` |
| `weaver_avx_768` | 3 | WEAVER-768 | `KAT_KEM_WeaverKEM-256.txt` |
| `weaver_avx_1024` | 5 | WEAVER-1024 | `KAT_KEM_WeaverKEM-512.txt` |

All three use `n=256` and AVX2 NTT when `USE_AVX_NTT=1` (default).

AVX2 assembly NTT (`src/avx/*.S`) can be enabled for modes 1/3 with `USE_AVX_NTT=1` (defines `WEAVER_USE_AVX_NTT`); keep it disabled unless `make test USE_AVX_NTT=1` is fully green.

Binaries are built with AVX2 flags for SIMD in `poly_avx.c` and future assembly hooks; the hot NTT path used for ICCS KAT is currently the same portable C as the reference.

## Correctness

1. **ICCS KAT** — replays all three `../Test_Vectors/KAT_KEM_WeaverKEM-*.txt` files (SM3-DRNG).
2. **Reference cross-check** — 100 stored vectors per level; `compare_ref_avx_{512,768,1024}` vs `ref/`.
3. **Random KEM derand** — 10 000 replay cases per level (`compare_ref_random_*`).
4. **AVX self-check** — `verify_vectors_{512,768,1024}` regenerates and matches stored AVX vectors.
5. **NTT primitives** — `compare_ntt_avx_{512,768,1024}` (scalar ref vs AVX assembly).

## Optimizations

**In use (default build):**

- `-mavx2` compile flags; portable C NTT matching `ref/src`
- `-march=native` + `-flto` (configurable via `PERF_NATIVE` / `ENABLE_LTO`)
- Portable C for Weaver-specific compression (10-bit PK / `u` for mode 3, 4-bit `v`), BCH, Inv_q lifting

**WIP (not linked by default):**

- AVX2 NTT / invNTT / basemul assembly (`src/avx/`)
- SIMD `poly_add` / `poly_sub` / `poly_reduce` via `WEAVER_AVX256_NTT` + `poly_avx.c`
