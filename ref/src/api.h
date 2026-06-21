#ifndef API_H
#define API_H

#include "params.h"

#define CRYPTO_SECRETKEYBYTES  WEAVER_SECRETKEYBYTES
#define CRYPTO_PUBLICKEYBYTES  WEAVER_PUBLICKEYBYTES
#define CRYPTO_CIPHERTEXTBYTES WEAVER_CIPHERTEXTBYTES
#define CRYPTO_BYTES           WEAVER_SSBYTES

// #if   (WEAVER_K == 2)
// #ifdef WEAVER_90S
// #define CRYPTO_ALGNAME "Kyber512-90s"
// #else
// #define CRYPTO_ALGNAME "Kyber512"
// #endif
// #elif (WEAVER_K == 3)
// #ifdef WEAVER_90S
// #define CRYPTO_ALGNAME "Kyber768-90s"
// #else
// #define CRYPTO_ALGNAME "Kyber768"
// #endif
// #elif (WEAVER_K == 4)
// #ifdef WEAVER_90S
// #define CRYPTO_ALGNAME "Kyber1024-90s"
// #else
// #define CRYPTO_ALGNAME "Kyber1024"
// #endif
// #endif

#if   (WEAVER_MODE == 1)
  #define CRYPTO_ALGNAME "WEAVER-640"
#elif (WEAVER_MODE == 3)
  #define CRYPTO_ALGNAME "WEAVER-1024"
#elif (WEAVER_MODE == 5)
  #define CRYPTO_ALGNAME "WEAVER-2048"
#else
  #error "WEAVER_MODE must be in {1,3,5}"
#endif

#define crypto_kem_keypair WEAVER_NAMESPACE(_keypair)
int crypto_kem_keypair(unsigned char *pk, unsigned char *sk);

#define crypto_kem_enc WEAVER_NAMESPACE(_enc)
int crypto_kem_enc(unsigned char *ct,
                   unsigned char *ss,
                   const unsigned char *pk);

#define crypto_kem_dec WEAVER_NAMESPACE(_dec)
int crypto_kem_dec(unsigned char *ss,
                   const unsigned char *ct,
                   const unsigned char *sk);

#endif
