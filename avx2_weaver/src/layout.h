#ifndef WEAVER_LAYOUT_H
#define WEAVER_LAYOUT_H

/*
 * AVX2 coefficient layout contract (see comments in params.h).
 *
 * mode 1/3 (n=256, WEAVER_AVX256_NTT):
 *   - AVX gen_matrix + rej_uniform_avx emit Kyber-AVX packed order.
 *   - poly_nttunpack is a layout adapter (not an NTT); required before basemul.
 *   - ntt_avx / basemul_avx use pq-crystals shuffle layout.
 *
 * mode 5 (n=512, WEAVER_USE_AVX_NTT512):
 *   - gen_matrix stays natural coefficient order (scalar rej_uniform).
 *   - ntt512_avx matches portable ntt.c order; poly_nttunpack is a no-op.
 *   - Do not enable WEAVER_AVX_GEN_MATRIX or Kyber packed layout on mode 5.
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
