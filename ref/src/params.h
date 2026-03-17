#ifndef PARAMS_H
#define PARAMS_H

#ifndef WEAVER_MODE
#define WEAVER_MODE 1
#endif

#define KYBER_K 4

#if   (WEAVER_MODE == 1)
  #define KYBER_N 128
  #define KYBER_NAMESPACE(s) pqcrystals_weaver512_ref##s
  #define KYBER_ETA1 4
  #define KYBER_ETA2 4
  #define KYBER_POLYCOMPRESSEDBYTES    ((KYBER_N * 4) / 8)
  #define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * ((KYBER_N * 9) / 8))

#elif (WEAVER_MODE == 3)
  #define KYBER_N 256
  #define KYBER_NAMESPACE(s) pqcrystals_weaver1024_ref##s
  #define KYBER_ETA1 2
  #define KYBER_ETA2 4
  #define KYBER_POLYCOMPRESSEDBYTES    ((KYBER_N * 4) / 8)
  #define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * ((KYBER_N * 10) / 8))

#elif (WEAVER_MODE == 5)
  #define KYBER_N 512
  #define KYBER_NAMESPACE(s) pqcrystals_weaver2048_ref##s
  #define KYBER_ETA1 1
  #define KYBER_ETA2 4
  #define KYBER_POLYCOMPRESSEDBYTES    ((KYBER_N * 6) / 8)
  #define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * ((KYBER_N * 10) / 8))

#else
  #error "WEAVER_MODE must be in {1,3,5}"
#endif

#define KYBER_POLYBYTES              ((KYBER_N * 12) / 8)
#define KYBER_Q                      3329
#define KYBER_SYMBYTES               32
#define KYBER_INDCPA_MSGBYTES        (KYBER_N / 8)
#define KYBER_POLYVECBYTES           (KYBER_K * KYBER_POLYBYTES)
#define KYBER_INDCPA_PUBLICKEYBYTES  (KYBER_POLYVECBYTES + KYBER_SYMBYTES)
#define KYBER_INDCPA_SECRETKEYBYTES  (KYBER_POLYVECBYTES)
#define KYBER_INDCPA_BYTES           (KYBER_POLYVECCOMPRESSEDBYTES + KYBER_POLYCOMPRESSEDBYTES)
#define KYBER_PUBLICKEYBYTES         (KYBER_INDCPA_PUBLICKEYBYTES)
#define KYBER_SECRETKEYBYTES         (KYBER_INDCPA_SECRETKEYBYTES \
                                      + KYBER_INDCPA_PUBLICKEYBYTES \
                                      + 2*KYBER_SYMBYTES)
#define KYBER_CIPHERTEXTBYTES        KYBER_INDCPA_BYTES
#define KYBER_SSBYTES                KYBER_INDCPA_MSGBYTES

#endif
