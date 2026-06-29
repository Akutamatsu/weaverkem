# WeaverKEM Implementations

## Directory Structure

```
Implementations/
├── Reference_Implementation/          # Portable reference implementation (ISO C)
│   ├── WeaverKEM-128/                # WEAVER_MODE=1, 128-bit classical security
│   ├── WeaverKEM-256/                # WEAVER_MODE=3, 256-bit classical security
│   ├── WeaverKEM-512/                # WEAVER_MODE=5, 512-bit classical security
│   ├── CMakeLists.txt                # Build configuration for reference
│   └── TESTING.md                    # Test instructions
├── Optimized_Implementation/          # AVX2-optimized implementation
│   ├── WeaverKEM-128/                # AVX2 optimized, 128-bit classical security
│   ├── WeaverKEM-256/                # AVX2 optimized, 256-bit classical security
│   ├── WeaverKEM-512/                # AVX2 optimized, 512-bit classical security
│   ├── cmake/                        # Shared CMake utilities
│   └── CMakeLists.txt                # Build configuration for optimized
└── README.md                         # This file
```

## Building the Implementations

### Reference Implementation (Portable, ISO C)

```bash
cd Reference_Implementation
mkdir build
cd build
cmake ..
make generate_kat
```

This generates KAT test vectors in `bin/output/KAT_KEM_*.txt`.

### Optimized Implementation (AVX2)

```bash
cd Optimized_Implementation
mkdir build
cd build
cmake ..
make generate_kat
```

This generates KAT test vectors using AVX2-optimized kernels.

### Verify Test Vectors Match

From the project root:

```bash
for NAME in WeaverKEM-128 WeaverKEM-256 WeaverKEM-512; do
    FILE1="Implementations/Reference_Implementation/build/bin/output/KAT_KEM_$NAME.txt"
    FILE2="Test_Vectors/KAT_KEM_$NAME.txt"
    if diff -q "$FILE1" "$FILE2" > /dev/null 2>&1; then
        echo "$NAME: PASS (output == Test_Vectors)"
    else
        echo "$NAME: FAIL (files differ)"
    fi
done
```

Expected output:
```
WeaverKEM-128: PASS (output == Test_Vectors)
WeaverKEM-256: PASS (output == Test_Vectors)
WeaverKEM-512: PASS (output == Test_Vectors)
```

## File Descriptions

### Files Provided by ICCS (SHALL NOT be modified)

The following files are provided by the Institute of Commercial Cryptography Standards
(ICCS) as part of the API_PKC submission framework and must not be modified:

| File | Description |
|------|-------------|
| `drng.c` | Deterministic Random Number Generator implementation (SM3-DRNG) |
| `drng.h` | DRNG header file and context declaration |
| `auxfunc.c` | Auxiliary cryptographic functions (SM3 hash, pseudoHash, pseudoXOF) |
| `auxfunc.h` | Auxiliary functions header |
| `KAT_KEM.c` | Official KAT generation program (generates test vector files) |

### WeaverKEM Algorithm-Specific Files

| File | Description |
|------|-------------|
| `api.h` | Public API header defining cryptographic interface |
| `KEM_WeaverKEM-XXX.h` | Algorithm instance configuration header; sets `ALGORITHM_INSTANCE` and `OUTPUT_BLANK_TEST_VECTORS` |
| `KEM_WeaverKEM-XXX.c` | Interface implementation; bridges WeaverKEM core to ICCS API; overrides `randombytes()` to use SM3-DRNG |
| `params.h` | Algorithm parameters (polynomial degree, modulus, compression rates, etc.) based on `WEAVER_MODE` |
| `kem.h` | KEM function declarations (keypair_derand, enc_derand, dec) |
| `symmetric.h` / `symmetric-iccs.c` | Hash/XOF interface layer using ICCS-provided `auxfunc` |
| `CMakeLists.txt` | Per-instance build configuration |

## Algorithm Parameter Sets

| Instance | WEAVER_MODE | Classical Security | Quantum Security | PK (bytes) | SK (bytes) | CT (bytes) | SS (bytes) |
|----------|-------------|-------------------|-----------------|------------|------------|------------|------------|
| WeaverKEM-128 | 1 | 128-bit | ≥80-bit | 752 | 1776 | 816 | 16 |
| WeaverKEM-256 | 3 | 256-bit | ≥128-bit | 1312 | 3072 | 1536 | 32 |
| WeaverKEM-512 | 5 | 512-bit | ≥256-bit | 2880 | 6400 | 3392 | 64 |

**Note:** Parameter sizes are determined at compile time by `params.h` based on `WEAVER_MODE` setting.
All three instances meet NGCC submission requirements:
- All instances support the required 128/256/512-bit classical security levels
- Quantum security margins meet or exceed NGCC requirements
- KEM encapsulated key length equals classical security level in bytes

## Implementation Details

### Common Core Algorithm (Both Reference and Optimized)

The actual WeaverKEM cryptographic operations are implemented in:
- `indcpa.c` / `indcpa.h` - IndCPA PKE scheme
- `kem_cca.c` - CCA2-secure KEM wrapper (Fujisaki-Okamoto)
- `ntt.c` / `ntt.h` - Number Theoretic Transform (reference implementation)
- `cbd.c` / `cbd.h` - Centered Binomial Distribution sampling
- `poly*.c` / `poly*.h` - Polynomial operations
- `msgenc.c` / `msgenc.h` - Message encoding/decoding
- `bch*.c` / `bch*.h` - BCH error-correcting codes
- `reduce.c` / `reduce.h` - Modular reduction utilities

### Reference Implementation

**Purpose:** Portable, unoptimized, easy-to-understand implementation for correctness verification.

**Characteristics:**
- Platform-independent ISO C99 code
- No architecture-specific optimizations
- Suitable for embedded systems and formal verification
- Includes all algorithm source files in each instance folder
- All randomness sourced from SM3-DRNG via `drng_algorithm`
- Uses ICCS-provided `auxfunc` for SM3 hash and XOF

**File count per instance:** ~45 files (full algorithm implementation + ICCS interface)

### Optimized Implementation (AVX2)

**Purpose:** High-performance implementation optimized for mainstream 64-bit x86-64 processors with AVX2 support.

**Characteristics:**
- AVX2 SIMD optimization for NTT, CBD, and polynomial compression
- Same NGCC API as Reference (`kem_keygen` / `kem_enc` / `kem_dec`)
- Same SM3-DRNG randomness source as Reference
- Uses identical ICCS-provided interfaces (`drng.c`, `auxfunc.c`, `KAT_KEM.c`)
- Core algorithm kernels linked from `../../avx2_weaver/src/`
- Each instance folder contains only interface files (minimal ~12 files per instance)

**AVX2 Kernels by Instance:**

| Instance | NTT Kernel | CBD Kernel | Compress Kernel |
|----------|-----------|-----------|-----------------|
| WeaverKEM-128 | `ntt3329_avx128.c` | `cbd_avx2.c` | `poly_compress9.c` (9-bit) |
| WeaverKEM-256 | `ntt7681_avx.c` | `cbd_avx2.c` | `poly_compress_avx.c` (10/8-bit) |
| WeaverKEM-512 | `ntt7681_avx.c` | `cbd_avx2.c` | `poly_compress_avx.c` (11/9-bit) |

**Build Instructions:**

```bash
cd Optimized_Implementation
mkdir build
cd build
cmake ..
cmake --build . --target generate_kat
```

**Optional CMake Flags:**
- `-DWEAVER_USE_AVX_NTT7681=OFF` - Disable ntt7681_avx kernel for modes 3/5 (default: ON)
- `-DWEAVER_USE_AVX_COMPRESS7681=OFF` - Disable poly_compress_avx for modes 3/5 (default: ON)

**Performance:**
All three instances undergo identical test-vector generation using the same deterministic SM3-DRNG,
ensuring bit-exact equivalence between Reference and Optimized outputs.

## Randomness and Reproducibility

### SM3-DRNG Deterministic Random Number Generator

All KEM operations (key generation, encapsulation) use randomness sourced exclusively from the
ICCS-provided SM3-DRNG (`drng_algorithm` context). This ensures:

- **Deterministic test vector generation:** Given the same DRNG seed, outputs are reproducible
- **Correctness verification:** Reference and Optimized implementations produce identical KAT files
- **Official compliance:** Uses ICCS-designated auxiliary functions per API_PKC specification

The `randombytes()` function in each `KEM_WeaverKEM-XXX.c` is overridden to call:
```c
int randombytes(unsigned char *x, unsigned long long xlen) {
    return get_random_number(&drng_algorithm, x, xlen);
}
```

### Hash and XOF Functions

Both Reference and Optimized implementations use ICCS-provided cryptographic functions:
- **SM3 hash:** via `auxfunc.c` function `sm3_256()`
- **Extendable Output Function (XOF):** via `auxfunc.c` function `pseudoXOF()`
- **Pseudo-hash:** via `auxfunc.c` function `pseudohash()`

These are integrated via `symmetric-iccs.c` which wraps the `auxfunc` interface.
The build system uses `-include symmetric.h` to override any conflicting hash definitions.

## Test Vector Verification

KAT (Known Answer Test) files are generated in the format specified by NGCC:
```
Test_Vectors/
├── KAT_KEM_WeaverKEM-128.txt
├── KAT_KEM_WeaverKEM-256.txt
└── KAT_KEM_WeaverKEM-512.txt
```

Each KAT file contains 100 test vectors covering:
- Key generation (public key, secret key)
- Encapsulation (ciphertext, shared secret)
- Decapsulation (shared secret recovery)

The format follows the NGCC KAT specification generated by the official `KAT_KEM.c` program.
