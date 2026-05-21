#ifndef API_H
#define API_H

#include "params.h"

#define CRYPTO_SECRETKEYBYTES  KYBER_SECRETKEYBYTES
#define CRYPTO_PUBLICKEYBYTES  KYBER_PUBLICKEYBYTES
#define CRYPTO_CIPHERTEXTBYTES KYBER_CIPHERTEXTBYTES
#define CRYPTO_BYTES           KYBER_SSBYTES

#if   (WEAVER_MODE == 1)
  #define CRYPTO_ALGNAME "WEAVER-512"
#elif (WEAVER_MODE == 3)
  #define CRYPTO_ALGNAME "WEAVER-768"
#elif (WEAVER_MODE == 5)
  #define CRYPTO_ALGNAME "WEAVER-1024"
#else
  #define CRYPTO_ALGNAME "WEAVER-UNKNOWN"
#endif

#define crypto_kem_keypair KYBER_NAMESPACE(_keypair)
int crypto_kem_keypair(unsigned char *pk, unsigned char *sk);

#define crypto_kem_enc KYBER_NAMESPACE(_enc)
int crypto_kem_enc(unsigned char *ct,
                   unsigned char *ss,
                   const unsigned char *pk);

#define crypto_kem_dec KYBER_NAMESPACE(_dec)
int crypto_kem_dec(unsigned char *ss,
                   const unsigned char *ct,
                   const unsigned char *sk);

#endif
