#ifndef KEM_H
#define KEM_H

#include "params.h"

#define crypto_kem_keypair_derand KYBER_NAMESPACE(_keypair_derand)
int crypto_kem_keypair_derand(uint8_t *pk, uint8_t *sk, const uint8_t *coins);

#define crypto_kem_keypair KYBER_NAMESPACE(_keypair)
int crypto_kem_keypair(uint8_t *pk, uint8_t *sk);

#define crypto_kem_enc_derand KYBER_NAMESPACE(_enc_derand)
int crypto_kem_enc_derand(uint8_t *ct, uint8_t *ss, const uint8_t *pk, const uint8_t *coins);

#define crypto_kem_enc KYBER_NAMESPACE(_enc)
int crypto_kem_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk);

#define crypto_kem_dec KYBER_NAMESPACE(_dec)
int crypto_kem_dec(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);

#endif
