# Section 5 benchmark results

## Environment
```
=== Environment ===
model name	: AMD EPYC 7C13 64-Core Processor
gcc (Debian 12.2.0-14+deb12u1) 12.2.0
6.1.0-44-amd64
```

mlkem commit: 4768bd3
Medians over 5 runs; N_test=1000 per run.

## Table 1 — End-to-end KEM (AVX2, median cycles)

**Updated:** `USE_GETRANDOM=1` (Linux `getrandom` for bench RNG); median of 5 runs.

| Scheme | n | k | Classical Sec. | KeyGen | Encaps | Decaps |
|--------|---|---|----------------|--------|--------|--------|
| ML-KEM-512 | 256 | 2 | 128-bit | 17,000 | 17,820 | 18,180 |
| Weaver-512 | 256 | 2 | 128-bit | 22,060 | 28,680 | 30,580 |
| ML-KEM-768 | 256 | 3 | 192-bit | 32,060 | 27,240 | 28,500 |
| ML-KEM-1024 | 256 | 4 | 256-bit | 38,740 | 39,060 | 41,500 |
| Weaver-768 | 256 | 3 | 192-bit | (see w768_avx.txt) | | |
| Weaver-1024 | 256 | 4 | 256-bit | 38,600 | 59,820 | 57,720 |

_Pre-`USE_GETRANDOM` Weaver AVX2 (NIST DRBG per 16-byte block): KeyGen 39,060 / 55,900 / 178,620 at (256,2), (256,4), (512,4)._

## Table 2 — Bandwidth (bytes)

| Scheme | |pk| (B) | |ct| (B) | |pk|+|ct| (B) |
|--------|-----------|-----------|-----------------|
| ML-KEM-512 | 800 | 768 | 1568 |
| Weaver-512 | 608 | 736 | 1344 |
| ML-KEM-768 | 1184 | 1088 | 2272 |
| ML-KEM-1024 | 1568 | 1568 | 3136 |
| Weaver-768 | 896 | 992 | 1888 |
| Weaver-1024 | 1184 | 1312 | 2496 |

## Table 3 — Weaver / ML-KEM cycle ratio (AVX2)

| Comparison | KeyGen | Encaps | Decaps | pk saving | ct saving |
|-----------|--------|--------|--------|-----------|-----------|
| Weaver-512 / ML-KEM-512 | 1.30× | 1.61× | 1.68× | 24% | 4% |
| Weaver-1024 / ML-KEM-1024 | 1.00× | 1.53× | 1.39× | 24% | 16% |

## Table 4 — AVX2 vs reference speedup

| Scheme | KeyGen | Encaps | Decaps |
|--------|--------|--------|--------|
| ML-KEM-512 | 4.51× | 5.23× | 7.01× |
| ML-KEM-1024 | 5.11× | 5.36× | 6.65× |
| Weaver-512 | 1.70× | 1.88× | 2.41× |
| Weaver-1024 | 2.65× | 2.30× | 2.95× |
| Weaver-1024 | 1.84× | 2.18× | 2.30× |

## Table 5 — Weaver components (AVX2)

| Component | W-512 | W-768 | W-1024 |
|-----------|-------|--------|--------|
| poly_ntt | 120 | 120 | 780 |
| poly_invntt | 120 | 120 | 1,160 |
| polyvec_basemul_acc | 120 | 220 | 6,160 |
| gen_matrix | 5,200 | 20,860 | 47,380 |
| kem_keypair | 39,060 | 55,900 | 178,620 |
| kem_encaps | 37,840 | 71,860 | 173,200 |
| kem_decaps | 30,720 | 57,720 | 178,620 |
