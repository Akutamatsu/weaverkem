#ifndef WEAVER_LAYOUT_H
#define WEAVER_LAYOUT_H

/* Modes 1/3/5 (n=256) use Kyber's AVX2 packed NTT layout.
 * gen_matrix applies poly_nttunpack before basemul.
 * Mode 7 (n=512) keeps standard coefficient order throughout. */

#endif
