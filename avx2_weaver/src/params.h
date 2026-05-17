#ifndef PARAMS_H
#define PARAMS_H

#ifndef WEAVER_MODE
#define WEAVER_MODE 3
#endif

#define PK_COMPRESS

#if (WEAVER_MODE == 1)
  #define KYBER_N 256
  #define KYBER_INDCPA_MSGBYTES 16
  #define KYBER_NAMESPACE(s) pqcrystals_weaver512_avx2##s
  #define KYBER_ETA1 5
  #define KYBER_ETA2 KYBER_ETA1
  #define KYBER_K 2
  #define KYBER_PK_POLYVECBYTES        (KYBER_K * ((KYBER_N * 9) / 8))
  #define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * ((KYBER_N * 9) / 8))
  #define KYBER_POLYCOMPRESSEDBYTES    ((KYBER_N * 4) / 8)

#elif (WEAVER_MODE == 3)
  #define KYBER_N 256
  #define KYBER_INDCPA_MSGBYTES       (KYBER_N / 8)
  #define KYBER_NAMESPACE(s) pqcrystals_weaver1024_avx2##s
  #define KYBER_ETA1 2
  #define KYBER_ETA2 KYBER_ETA1
  #define KYBER_K 4
  /* ICCS Test_Vectors/KAT_KEM_WeaverKEM-256.txt: PK 1312, CT 1408, SK 2912 → du=10, dv=4 */
  #define KYBER_PK_POLYVECBYTES        (KYBER_K * ((KYBER_N * 10) / 8))
  #define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * ((KYBER_N * 10) / 8))
  #define KYBER_POLYCOMPRESSEDBYTES    ((KYBER_N * 4) / 8)

#elif (WEAVER_MODE == 5)
  #define KYBER_N 512
  #define KYBER_INDCPA_MSGBYTES       (KYBER_N / 8)
  #define KYBER_NAMESPACE(s) pqcrystals_weaver2048_avx2##s
  #define KYBER_ETA1 1
  #define KYBER_ETA2 KYBER_ETA1
  #define KYBER_K 4
  #define KYBER_PK_POLYVECBYTES        (KYBER_K * ((KYBER_N * 9) / 8))
  #define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * ((KYBER_N * 9) / 8))
  #define KYBER_POLYCOMPRESSEDBYTES    ((KYBER_N * 6) / 8)

#else
  #error "WEAVER_MODE must be in {1,3,5}"
#endif

/* Optional AVX2 NTT/basemul for n=256 (modes 1 and 3). Enable with -DWEAVER_USE_AVX_NTT. */
#if !defined(WEAVER_AVX256_NTT) && (KYBER_N == 256) && defined(WEAVER_USE_AVX_NTT)
#define WEAVER_AVX256_NTT 1
#endif

#if defined(WEAVER_AVX256_NTT)
#define WEAVER_AVX_GEN_MATRIX 1
#define WEAVER_USE_AVX_COMPRESS 1
#endif

/* n=512 (mode 5): AVX2 NTT + fq reduce/tomont */
#if (KYBER_N == 512) && defined(WEAVER_USE_AVX_FQ_512)
#define WEAVER_USE_AVX_NTT512 1
#endif

#define KYBER_Q     3329
#define KYBER_HALFQ ((KYBER_Q + 1) / 2)

#define KYBER_SYMBYTES 32
#define KYBER_SSBYTES  KYBER_INDCPA_MSGBYTES

#define KYBER_POLYBYTES     ((KYBER_N * 12) / 8)
#define KYBER_POLYVECBYTES  (KYBER_K * KYBER_POLYBYTES)

#ifndef PK_COMPRESS
#undef KYBER_PK_POLYVECBYTES
#define KYBER_PK_POLYVECBYTES KYBER_POLYVECBYTES
#endif

#define KYBER_INDCPA_PUBLICKEYBYTES (KYBER_PK_POLYVECBYTES + KYBER_SYMBYTES)
#define KYBER_INDCPA_SECRETKEYBYTES (KYBER_POLYVECBYTES)
#define KYBER_INDCPA_BYTES          (KYBER_POLYVECCOMPRESSEDBYTES + KYBER_POLYCOMPRESSEDBYTES)

#define KYBER_PUBLICKEYBYTES  (KYBER_INDCPA_PUBLICKEYBYTES)
#define KYBER_SECRETKEYBYTES  (KYBER_INDCPA_SECRETKEYBYTES \
                               + KYBER_INDCPA_PUBLICKEYBYTES \
                               + 2*KYBER_SYMBYTES)
#define KYBER_CIPHERTEXTBYTES  KYBER_INDCPA_BYTES

#include "layout.h"

#endif
