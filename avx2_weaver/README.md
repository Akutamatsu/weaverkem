# WeaverKEM AVX2 Optimized Implementation

AVX2-oriented build of WeaverKEM, based on `ref/src/`. Algorithm logic matches the ICCS Reference_Implementation; `Test_Vectors/` is generated from that tree.

## Layout

- `src/` — Weaver algorithm (indcpa, kem, BCH, msgenc, polyvec) + Weaver-specific AVX2 kernels
- `src/avx/` — optional q=7681 NTT assembly (`ntt7681_asm256.S`, experimental)
- `src/keccak4x/` — Keccak-f[1600]×4 (from Kyber/PQClean lineage) for parallel SHAKE128
- `tools/` — KAT verification and extended test vectors
- `bench/` — Cycle-count benchmarks

## Code map (copied vs optimized vs removed)

| Category | Files | Notes |
|----------|-------|-------|
| **Reference C (fallback)** | `ntt.c`, `reduce.c`, `cbd.c`, `indcpa.c`, … | Same logic as `ref/src/`; used when AVX flags are off |
| **Copied / adapted** | `keccak4x/*`, `fips202x4.c` | Kyber/PQClean Keccak×4; wired by `indcpa_gen_matrix_avx128.c` / `avx7681.c` |
| **Weaver AVX2 (default ON)** | `ntt3329_avx128.c`, `ntt7681_avx.c`, `cbd_avx2.c`, `poly_compress_avx.c`, `poly_compress9.c` (mode 1 only), `indcpa_gen_matrix_avx128.c`, `indcpa_gen_matrix_avx7681.c`, `rejsample.c` | Parameter-specific SIMD for NGCC Table 2 sets |
| **Experimental (OFF)** | `avx/ntt7681_asm256.S`, `avx/consts7681.c` | `USE_AVX_NTT7681_ASM=1` |
| **Removed (dead code)** | Kyber `avx/{ntt,invntt,basemul,fq,shuffle}.S`, `poly_avx.c`, `poly_avx512.c`, `indcpa_gen_matrix_avx.c`, `poly_weaver.c`, `kem_cpa.c`, `rng.c` | Legacy Kyber n=256 paths; never used for q=7681 / n=128 |

## Build

Requires **x86_64** with AVX2 (`-mavx2 -mbmi2 -mpopcnt`). On Linux, `gcc` or `clang` is auto-selected.

### Make (default)

```bash
cd avx2_weaver
make all      # binaries in bin/
make kat      # replay ../Test_Vectors/ (must match ref; all 3 security levels)
make test     # make kat + extended ref/AVX cross-check vectors
make bench    # keypair / encaps / decaps timing
```

### CMake

```bash
cd avx2_weaver
cmake -B build -S .
cmake --build build
ctest --test-dir build                    # compress differential tests
cmake --build build --target weaver_kat   # ICCS KAT replay
cmake --build build --target weaver_bench # speed_test (512/1024/2048)
cmake --build build --target weaver_quick_check
```

Useful CMake options: `-DWEAVER_PERF_NATIVE=OFF`, `-DWEAVER_ENABLE_LTO=OFF`, `-DWEAVER_USE_AVX_CBD=OFF`, `-DWEAVER_USE_AVX_COMPRESS7681=OFF`.

NGCC optimized submission: see `Implementations/Optimized_Implementation/` (ICCS `kem_*` + SM3-DRNG + `auxfunc`; `cmake --build build-opt --target verify_kat`).

Binaries land in `build/bin/`.

Override the compiler if needed: `make CC=gcc all`.
Performance toggles:

- `make PERF_NATIVE=1 ENABLE_LTO=1 bench` (default): `-march=native` + LTO for host CPU.
- `make PERF_NATIVE=0 ENABLE_LTO=0 bench`: conservative portable flags.
- Per-component off switches: `USE_AVX_NTT128=0`, `USE_AVX_NTT7681=0`, `USE_AVX_COMPRESS7681=0`, etc.

## Parameter sets

| Binary | WEAVER_MODE | Paper name | Default hot path |
|--------|-------------|------------|------------------|
| `weaver_avx_512` | 1 | WEAVER-640 | `ntt3329_avx128` + AVX gen_matrix + 9-bit compress |
| `weaver_avx_1024` | 3 | WEAVER-1024 | `ntt7681_avx` + AVX gen_matrix + 10-bit compress |
| `weaver_avx_2048` | 5 | WEAVER-2048 | `ntt7681_avx` (n=512) + AVX gen_matrix + 11-bit compress |

Set `USE_AVX_NTT128=0` / `USE_AVX_NTT7681=0` to fall back to portable C NTT in `src/ntt.c` for benchmarking or debugging.

## Correctness

1. **ICCS KAT** — `make kat` replays all three `../Test_Vectors/KAT_KEM_WeaverKEM-*.txt` files (SM3-DRNG via `kat_iccs_128` / `kat_iccs_256` / `kat_iccs_512`). These files are byte-identical to `Implementations/Reference_Implementation` output (`make generate_kat`).
2. **Reference cross-check** — `gen_vectors_ref_*` builds 100 vectors from `ref/`; `compare_ref_avx_*` checks byte-identical outputs from this tree.
3. **Self-test** — `gen_vectors_*` + `verify_vectors_*` on extended seeds.

## Optimizations (default build)

- **NTT / basemul / reduce:** `ntt3329_avx128.c` (mode 1, n=128), `ntt7681_avx.c` (modes 3/5, q=7681)
- **CBD:** `cbd_avx2.c` for η=7/9 (modes 3/5)
- **Compress:** `poly_compress_avx.c` — 9-bit (mode 1), 10/11-bit PK and 8/9-bit v (modes 3/5)
- **gen_matrix:** SHAKE128×4 + `rej_uniform_avx` (mode 1) or `rej_uniform7681` (modes 3/5)
- **Build flags:** `-mavx2 -mbmi2 -mpopcnt`, optional `-march=native` + LTO
- **Still portable C:** BCH/msgenc core, Inv_q lifting, poly add/sub
