#ifndef WEAVER_LAYOUT_H
#define WEAVER_LAYOUT_H

/*
 * AVX2 coefficient layout contract (all modes use n=256).
 *
 * WEAVER_AVX256_NTT:
 *   - AVX gen_matrix + rej_uniform_avx emit Kyber-AVX packed order.
 *   - poly_nttunpack is a layout adapter (not an NTT); required before basemul.
 *   - ntt_avx / basemul_avx use pq-crystals shuffle layout.
 */

#if defined(WEAVER_AVX256_NTT)
#define WEAVER_MATRIX_NEEDS_NTTUNPACK 1
#else
#define WEAVER_MATRIX_NEEDS_NTTUNPACK 0
#endif

#endif
