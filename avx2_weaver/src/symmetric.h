#ifndef SYMMETRIC_H
#define SYMMETRIC_H

#include <stddef.h>
#include <stdint.h>
#include "params.h"

#include "fips202.h"

typedef keccak_state xof_state;

typedef struct {
  uint8_t bytes[WEAVER_HBYTES];
} weaver_hbuf;

typedef struct {
  uint8_t bytes[WEAVER_GBYTES];
} weaver_gbuf;

#define weaver_seedexpbytes (2 * WEAVER_SYMBYTES)

typedef struct {
  uint8_t bytes[weaver_seedexpbytes];
} weaver_seedexpbuf;

#define weaver_hash_h WEAVER_NAMESPACE(weaver_hash_h)
void weaver_hash_h(uint8_t out[WEAVER_HBYTES], const uint8_t *in, size_t inlen);

#define weaver_hash_g WEAVER_NAMESPACE(weaver_hash_g)
void weaver_hash_g(uint8_t out[WEAVER_GBYTES], const uint8_t *in, size_t inlen);

#define weaver_expand_keypair_seeds WEAVER_NAMESPACE(weaver_expand_keypair_seeds)
void weaver_expand_keypair_seeds(uint8_t out[weaver_seedexpbytes], const uint8_t *in, size_t inlen);

#define weaver_shake128_absorb WEAVER_NAMESPACE(weaver_shake128_absorb)
void weaver_shake128_absorb(keccak_state *s,
                           const uint8_t seed[WEAVER_SYMBYTES],
                           uint8_t x,
                           uint8_t y);

#define weaver_shake256_prf WEAVER_NAMESPACE(weaver_shake256_prf)
void weaver_shake256_prf(uint8_t *out, size_t outlen, const uint8_t key[WEAVER_SYMBYTES], uint8_t nonce);

#define weaver_shake256_rkprf WEAVER_NAMESPACE(weaver_shake256_rkprf)
void weaver_shake256_rkprf(uint8_t out[WEAVER_SSBYTES], const uint8_t key[WEAVER_SYMBYTES], const uint8_t input[WEAVER_CIPHERTEXTBYTES]);

#define XOF_BLOCKBYTES SHAKE128_RATE

#define hash_h(OUT, IN, INBYTES) weaver_hash_h((OUT), (IN), (INBYTES))
#define hash_g(OUT, IN, INBYTES) weaver_hash_g((OUT), (IN), (INBYTES))
#define expand_keypair_seeds(OUT, IN, INBYTES) weaver_expand_keypair_seeds((OUT), (IN), (INBYTES))
#define xof_absorb(STATE, SEED, X, Y) weaver_shake128_absorb(STATE, SEED, X, Y)
#define xof_squeezeblocks(OUT, OUTBLOCKS, STATE) shake128_squeezeblocks(OUT, OUTBLOCKS, STATE)
#define prf(OUT, OUTBYTES, KEY, NONCE) weaver_shake256_prf(OUT, OUTBYTES, KEY, NONCE)
#define rkprf(OUT, KEY, INPUT) weaver_shake256_rkprf(OUT, KEY, INPUT)

#endif /* SYMMETRIC_H */
