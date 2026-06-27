#ifndef WEAVER_LAYOUT_H
#define WEAVER_LAYOUT_H

/*
 * AVX2 coefficient layout contract.
 *
 * Kyber AVX (n=256, q=3329): packed gen_matrix + poly_nttunpack before basemul.
 * NGCC modes use portable ref C NTT until q7681/n128 AVX paths are integrated.
 */

#if (WEAVER_MODE == 5) && defined(WEAVER_AVX_GEN_MATRIX)
#error "Mode 5 must not use Kyber packed gen_matrix (standard coefficient order only)"
#endif

#if defined(WEAVER_AVX256_NTT)
#define WEAVER_MATRIX_NEEDS_NTTUNPACK 1
#else
#define WEAVER_MATRIX_NEEDS_NTTUNPACK 0
#endif

#endif
