/*
 * ICCS symmetric layer for this algorithm instance (shadows ref/src/symmetric.h).
 * Hash/XOF via auxfunc (sm3hash, pseudohash, pseudoXOF).
 */
#ifndef SYMMETRIC_H
#define SYMMETRIC_H

#include <stddef.h>
#include <stdint.h>
#include "params.h"

#if WEAVER_MODE == 1
#define XOF_BLOCKBYTES 168
#else
#define XOF_BLOCKBYTES 136
#endif
#define SHAKE256_RATE 136

typedef struct {
  uint8_t msg[WEAVER_SYMBYTES + 2];
  size_t msg_len;
  size_t pos;
} xof_state;

#define weaver_iccs_xof_absorb_wrap WEAVER_NAMESPACE(weaver_iccs_xof_absorb_wrap)
void weaver_iccs_xof_absorb_wrap(xof_state *state,
                                 const uint8_t seed[WEAVER_SYMBYTES],
                                 uint8_t x,
                                 uint8_t y);

#define weaver_iccs_xof_squeezeblocks_wrap WEAVER_NAMESPACE(weaver_iccs_xof_squeezeblocks_wrap)
void weaver_iccs_xof_squeezeblocks_wrap(uint8_t *out, size_t nblocks, xof_state *state);

#define weaver_iccs_hash_h WEAVER_NAMESPACE(weaver_iccs_hash_h)
void weaver_iccs_hash_h(uint8_t *out, const uint8_t *in, size_t inlen);

#define weaver_iccs_hash_g WEAVER_NAMESPACE(weaver_iccs_hash_g)
void weaver_iccs_hash_g(uint8_t *out, const uint8_t *in, size_t inlen);

#define weaver_iccs_expand_keypair_seeds WEAVER_NAMESPACE(weaver_iccs_expand_keypair_seeds)
void weaver_iccs_expand_keypair_seeds(uint8_t *out, const uint8_t *in, size_t inlen);

#define weaver_iccs_hash_kr WEAVER_NAMESPACE(weaver_iccs_hash_kr)
void weaver_iccs_hash_kr(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen);

#define weaver_iccs_prf WEAVER_NAMESPACE(weaver_iccs_prf)
void weaver_iccs_prf(uint8_t *out, size_t outlen, const uint8_t key[WEAVER_SYMBYTES], uint8_t nonce);

#define weaver_iccs_rkprf WEAVER_NAMESPACE(weaver_iccs_rkprf)
void weaver_iccs_rkprf(uint8_t out[WEAVER_SSBYTES],
                       const uint8_t key[WEAVER_SYMBYTES],
                       const uint8_t input[WEAVER_CIPHERTEXTBYTES]);

#define hash_h(OUT, IN, INBYTES) weaver_iccs_hash_h(OUT, IN, INBYTES)
#define hash_g(OUT, IN, INBYTES) weaver_iccs_hash_g(OUT, IN, INBYTES)
#define expand_keypair_seeds(OUT, IN, INBYTES) weaver_iccs_expand_keypair_seeds(OUT, IN, INBYTES)
#define xof_absorb(STATE, SEED, X, Y) weaver_iccs_xof_absorb_wrap(STATE, SEED, X, Y)
#define xof_squeezeblocks(OUT, OUTBLOCKS, STATE) weaver_iccs_xof_squeezeblocks_wrap(OUT, OUTBLOCKS, STATE)
#define prf(OUT, OUTBYTES, KEY, NONCE) weaver_iccs_prf(OUT, OUTBYTES, KEY, NONCE)
#define rkprf(OUT, KEY, INPUT) weaver_iccs_rkprf(OUT, KEY, INPUT)

#define shake256(OUT, OUTLEN, IN, INLEN) weaver_iccs_hash_kr((OUT), (OUTLEN), (IN), (INLEN))

#endif /* SYMMETRIC_H */
