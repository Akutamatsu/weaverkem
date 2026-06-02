#ifndef PARAMS_H
#define PARAMS_H

#ifndef WEAVER_MODE
#define WEAVER_MODE 3
#endif

#define PK_COMPRESS

#if (WEAVER_MODE == 1)
  #define WEAVER_N 256
  #define WEAVER_INDCPA_MSGBYTES 16
  #define WEAVER_NAMESPACE(s) pqcrystals_weaver512_avx2##s
  #define WEAVER_ETA1 5
  #define WEAVER_ETA2 WEAVER_ETA1
  #define WEAVER_K 2
  #define WEAVER_PK_POLYVECBYTES        (WEAVER_K * ((WEAVER_N * 9) / 8))
  #define WEAVER_POLYVECCOMPRESSEDBYTES (WEAVER_K * ((WEAVER_N * 9) / 8))
  #define WEAVER_POLYCOMPRESSEDBYTES    ((WEAVER_N * 4) / 8)

#elif (WEAVER_MODE == 3)
  #define WEAVER_N 256
  #define WEAVER_INDCPA_MSGBYTES       (WEAVER_N / 8)
  #define WEAVER_NAMESPACE(s) pqcrystals_weaver1024_avx2##s
  #define WEAVER_ETA1 2
  #define WEAVER_ETA2 WEAVER_ETA1
  #define WEAVER_K 4
  /* ICCS Test_Vectors/KAT_KEM_WeaverKEM-256.txt: PK 1312, CT 1408, SK 2912 → du=10, dv=4 */
  #define WEAVER_PK_POLYVECBYTES        (WEAVER_K * ((WEAVER_N * 10) / 8))
  #define WEAVER_POLYVECCOMPRESSEDBYTES (WEAVER_K * ((WEAVER_N * 10) / 8))
  #define WEAVER_POLYCOMPRESSEDBYTES    ((WEAVER_N * 4) / 8)

#elif (WEAVER_MODE == 5)
  #define WEAVER_N 512
  #define WEAVER_INDCPA_MSGBYTES       (WEAVER_N / 8)
  #define WEAVER_NAMESPACE(s) pqcrystals_weaver2048_avx2##s
  #define WEAVER_ETA1 1
  #define WEAVER_ETA2 WEAVER_ETA1
  #define WEAVER_K 4
  #define WEAVER_PK_POLYVECBYTES        (WEAVER_K * ((WEAVER_N * 9) / 8))
  #define WEAVER_POLYVECCOMPRESSEDBYTES (WEAVER_K * ((WEAVER_N * 9) / 8))
  #define WEAVER_POLYCOMPRESSEDBYTES    ((WEAVER_N * 6) / 8)

#else
  #error "WEAVER_MODE must be in {1,3,5}"
#endif

/* Optional AVX2 NTT/basemul for n=256 (modes 1 and 3). Enable with -DWEAVER_USE_AVX_NTT. */
#if !defined(WEAVER_AVX256_NTT) && (WEAVER_N == 256) && defined(WEAVER_USE_AVX_NTT)
#define WEAVER_AVX256_NTT 1
#endif

#if defined(WEAVER_AVX256_NTT)
#define WEAVER_AVX_GEN_MATRIX 1
#define WEAVER_USE_AVX_COMPRESS 1
#endif

/* n=512 (mode 5): AVX2 NTT + fq reduce/tomont */
#if (WEAVER_N == 512) && defined(WEAVER_USE_AVX_FQ_512)
#define WEAVER_USE_AVX_NTT512 1
#endif

/* n=512 (mode 5): parallel SHAKE128x4 matrix gen (standard coefficient order). */
#if (WEAVER_N == 512) && defined(WEAVER_AVX_GEN_MATRIX512)
/* Must not use Kyber packed gen_matrix on mode 5 (see layout.h). */
#endif

#define WEAVER_Q     3329
#define WEAVER_HALFQ ((WEAVER_Q + 1) / 2)

#define WEAVER_SYMBYTES 32
/* FO message m for crypto_kem_enc_derand; equals INDCPA message length per mode. */
#define WEAVER_KEM_DERAND_COINBYTES WEAVER_INDCPA_MSGBYTES
#define WEAVER_SSBYTES  WEAVER_INDCPA_MSGBYTES

#define WEAVER_POLYBYTES     ((WEAVER_N * 12) / 8)
#define WEAVER_POLYVECBYTES  (WEAVER_K * WEAVER_POLYBYTES)

#ifndef PK_COMPRESS
#undef WEAVER_PK_POLYVECBYTES
#define WEAVER_PK_POLYVECBYTES WEAVER_POLYVECBYTES
#endif

#define WEAVER_INDCPA_PUBLICKEYBYTES (WEAVER_PK_POLYVECBYTES + WEAVER_SYMBYTES)
#define WEAVER_INDCPA_SECRETKEYBYTES (WEAVER_POLYVECBYTES)
#define WEAVER_INDCPA_BYTES          (WEAVER_POLYVECCOMPRESSEDBYTES + WEAVER_POLYCOMPRESSEDBYTES)

#define WEAVER_PUBLICKEYBYTES  (WEAVER_INDCPA_PUBLICKEYBYTES)
#define WEAVER_SECRETKEYBYTES  (WEAVER_INDCPA_SECRETKEYBYTES \
                               + WEAVER_INDCPA_PUBLICKEYBYTES \
                               + 2*WEAVER_SYMBYTES)
#define WEAVER_CIPHERTEXTBYTES  WEAVER_INDCPA_BYTES

#include "layout.h"

#endif
