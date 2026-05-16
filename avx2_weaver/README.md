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
make test     # make kat + extended ref/AVX cross-check vectors
make bench    # keypair / encaps / decaps timing
```

Override the compiler if needed: `make CC=gcc all`.
Performance toggles:

- `make PERF_NATIVE=1 ENABLE_LTO=1 bench` (default): `-march=native` + LTO for host CPU.
- `make PERF_NATIVE=0 ENABLE_LTO=0 bench`: conservative portable flags.
- `make USE_AVX_NTT=1 test`: tries AVX2 NTT path for modes 1/3 (must pass vector checks before use).

## Parameter sets

| Binary | WEAVER_MODE | Paper name | NTT in default build |
|--------|-------------|------------|----------------------|
| `weaver_avx_512` | 1 | WEAVER-512 | portable C (`src/ntt.c`) |
| `weaver_avx_1024` | 3 | WEAVER-1024 | portable C (`src/ntt.c`) |
| `weaver_avx_2048` | 5 | WEAVER-2048 | portable C (`src/ntt.c`, n=512) |

AVX2 assembly NTT (`src/avx/*.S`) can be enabled for modes 1/3 with `USE_AVX_NTT=1` (defines `WEAVER_USE_AVX_NTT`); keep it disabled unless `make test USE_AVX_NTT=1` is fully green.

Binaries are built with AVX2 flags for SIMD in `poly_avx.c` and future assembly hooks; the hot NTT path used for ICCS KAT is currently the same portable C as the reference.

## Correctness

1. **ICCS KAT** — `make kat` replays all three `../Test_Vectors/KAT_KEM_WeaverKEM-*.txt` files (SM3-DRNG via `kat_iccs_128` / `kat_iccs_256` / `kat_iccs_512`). These files are byte-identical to `Implementations/Reference_Implementation` output (`make generate_kat`).
2. **Reference cross-check** — `gen_vectors_ref_*` builds 100 vectors from `ref/`; `compare_ref_avx_*` checks byte-identical outputs from this tree.
3. **Self-test** — `gen_vectors_*` + `verify_vectors_*` on extended seeds.

## Optimizations

**In use (default build):**

- `-mavx2` compile flags; portable C NTT matching `ref/src`
- `-march=native` + `-flto` (configurable via `PERF_NATIVE` / `ENABLE_LTO`)
- Portable C for Weaver-specific compression (10-bit PK / `u` for mode 3, 4-bit `v`), BCH, Inv_q lifting

**WIP (not linked by default):**

- AVX2 NTT / invNTT / basemul assembly (`src/avx/`)
- SIMD `poly_add` / `poly_sub` / `poly_reduce` via `WEAVER_AVX256_NTT` + `poly_avx.c`
