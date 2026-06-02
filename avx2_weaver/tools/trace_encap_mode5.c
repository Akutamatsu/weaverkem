/* Mode-5 encaps trace: print FNV-1a digests at each indcpa_enc stage (ref or AVX build). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "params.h"
#include "indcpa.h"
#include "poly.h"
#include "polyvec.h"
#include "msgenc.h"
#include "symmetric.h"
#include "kem.h"

#if WEAVER_REF_SIDE
#define IMPL_TAG "ref"
#else
#define IMPL_TAG "avx"
#endif
#include "rng.h"

static uint64_t fnv1a_bytes(const uint8_t *p, size_t n)
{
  uint64_t h = 14695981039346656037ULL;
  size_t i;
  for(i = 0; i < n; i++) {
    h ^= p[i];
    h *= 1099511628211ULL;
  }
  return h;
}

static uint64_t fnv1a_poly(const poly *p)
{
  return fnv1a_bytes((const uint8_t *)p->coeffs, sizeof(p->coeffs));
}

static uint64_t fnv1a_polyvec(const polyvec *pv)
{
  uint64_t h = 14695981039346656037ULL;
  unsigned int i;
  for(i = 0; i < WEAVER_K; i++) {
    uint64_t t = fnv1a_poly(&pv->vec[i]);
    h ^= t;
    h *= 1099511628211ULL;
  }
  return h;
}

static uint64_t fnv1a_matrix(polyvec *at)
{
  uint64_t h = 14695981039346656037ULL;
  unsigned int i, j;
  for(i = 0; i < WEAVER_K; i++) {
    for(j = 0; j < WEAVER_K; j++) {
      uint64_t t = fnv1a_poly(&at[i].vec[j]);
      h ^= t;
      h *= 1099511628211ULL;
    }
  }
  return h;
}

static void trace_line(const char *label, uint64_t digest)
{
  printf("TRACE mode=5 impl=%s %-28s digest=%016llx\n",
         IMPL_TAG, label, (unsigned long long)digest);
}

static void trace_encap(const uint8_t m[WEAVER_INDCPA_MSGBYTES],
                        const uint8_t pk[WEAVER_INDCPA_PUBLICKEYBYTES],
                        const uint8_t coins[WEAVER_SYMBYTES])
{
  unsigned int i;
  uint8_t seed[WEAVER_SYMBYTES];
  uint8_t nonce = 0;
  polyvec sp = {0}, pkpv = {0}, at[WEAVER_K] = {0}, b = {0};
  poly v = {0}, k = {0};
  uint8_t c[WEAVER_INDCPA_BYTES];
  uint8_t c1[WEAVER_POLYVECCOMPRESSEDBYTES];
  uint8_t c2[WEAVER_POLYCOMPRESSEDBYTES];

  trace_line("coins", fnv1a_bytes(coins, WEAVER_SYMBYTES));

  unpack_pk(&pkpv, seed, pk);
  trace_line("pkpv_unpack", fnv1a_polyvec(&pkpv));
  trace_line("public_seed", fnv1a_bytes(seed, WEAVER_SYMBYTES));

  polyvec_ntt(&pkpv);
  trace_line("pkpv_ntt", fnv1a_polyvec(&pkpv));

  poly_frommsg(&k, m);
  trace_line("msg_poly", fnv1a_poly(&k));

  gen_matrix(at, seed, 1);
  trace_line("at_matrix", fnv1a_matrix(at));

  for(i = 0; i < WEAVER_K; i++)
    poly_getnoise_eta1(&sp.vec[i], coins, nonce++);
  trace_line("sp_time", fnv1a_polyvec(&sp));

  polyvec_ntt(&sp);
  trace_line("sp_ntt", fnv1a_polyvec(&sp));

  for(i = 0; i < WEAVER_K; i++)
    polyvec_basemul_acc_montgomery(&b.vec[i], &at[i], &sp);
  trace_line("b_ntt_acc", fnv1a_polyvec(&b));

  polyvec_basemul_acc_montgomery(&v, &pkpv, &sp);
  trace_line("v_ntt_acc", fnv1a_poly(&v));

  polyvec_invntt_tomont(&b);
  trace_line("b_invntt", fnv1a_polyvec(&b));

  poly_invntt_tomont(&v);
  trace_line("v_invntt", fnv1a_poly(&v));

  poly_add(&v, &v, &k);
  trace_line("v_add_msg", fnv1a_poly(&v));

  polyvec_reduce(&b);
  poly_reduce(&v);
  trace_line("b_reduce", fnv1a_polyvec(&b));
  trace_line("v_reduce", fnv1a_poly(&v));

  polyvec_compress(c1, &b);
  poly_compress(c2, &v);
  trace_line("c1", fnv1a_bytes(c1, sizeof(c1)));
  trace_line("c2", fnv1a_bytes(c2, sizeof(c2)));

  memcpy(c, c1, sizeof(c1));
  memcpy(c + sizeof(c1), c2, sizeof(c2));
  trace_line("ct", fnv1a_bytes(c, sizeof(c)));
}

int main(int argc, char **argv)
{
  unsigned int i;
  int use_vector0 = (argc > 1 && strcmp(argv[1], "vector0") == 0);
  uint8_t kp_coins[WEAVER_SYMBYTES];
  uint8_t enc_coins[WEAVER_SYMBYTES];
  uint8_t replay_coins[2 * WEAVER_SYMBYTES];
  uint8_t replay_enc[WEAVER_SYMBYTES];
  uint8_t buf[WEAVER_INDCPA_MSGBYTES + WEAVER_SYMBYTES];
  uint8_t kr[WEAVER_SSBYTES + WEAVER_SYMBYTES];
  uint8_t pk[WEAVER_PUBLICKEYBYTES];
  uint8_t sk[WEAVER_SECRETKEYBYTES];
  uint32_t s = 0xC0FFEE42;

  for(i = 0; i < WEAVER_SYMBYTES; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    kp_coins[i] = (uint8_t)s;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    enc_coins[i] = (uint8_t)s;
  }

  s = 0xC0FFEE42;
  for(i = 0; i < sizeof(replay_coins); i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    replay_coins[i] = (uint8_t)s;
  }
  for(i = 0; i < sizeof(replay_enc); i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    replay_enc[i] = (uint8_t)s;
  }

  if(use_vector0) {
    uint8_t seed[32] = {0,0,0,0, 0x57,0x56,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    uint8_t enc_in[WEAVER_SYMBYTES];
    uint8_t ct[WEAVER_CIPHERTEXTBYTES];
    uint8_t ss[WEAVER_SSBYTES];

    randombytes_init(seed, NULL, 256);
    randombytes(replay_coins, sizeof(replay_coins));
    crypto_kem_keypair_derand(pk, sk, replay_coins);
    trace_line("vec0_pk", fnv1a_bytes(pk, sizeof(pk)));
    randombytes(enc_in, sizeof(enc_in));
    crypto_kem_enc_derand(ct, ss, pk, enc_in);
    trace_line("vec0_ct", fnv1a_bytes(ct, sizeof(ct)));
  }

  crypto_kem_keypair_derand(pk, sk, replay_coins);
  trace_line("replay_pk", fnv1a_bytes(pk, WEAVER_PUBLICKEYBYTES));
  trace_line("replay_sk", fnv1a_bytes(sk, WEAVER_SECRETKEYBYTES));
  {
    uint8_t ct[WEAVER_CIPHERTEXTBYTES];
    uint8_t ss[WEAVER_SSBYTES];
    uint8_t kbuf[WEAVER_INDCPA_MSGBYTES + WEAVER_SYMBYTES];
    uint8_t kkr[WEAVER_SSBYTES + WEAVER_SYMBYTES];

    memcpy(kbuf, replay_enc, WEAVER_SYMBYTES);
    memset(kbuf + WEAVER_SYMBYTES, 0, WEAVER_INDCPA_MSGBYTES - WEAVER_SYMBYTES);
    trace_line("replay_kbuf_pre_h", fnv1a_bytes(kbuf, sizeof(kbuf)));
    hash_h(kbuf + WEAVER_INDCPA_MSGBYTES, pk, WEAVER_PUBLICKEYBYTES);
    trace_line("replay_kbuf_post_h", fnv1a_bytes(kbuf, sizeof(kbuf)));
    shake256(kkr, sizeof(kkr), kbuf, sizeof(kbuf));
    trace_line("replay_kr", fnv1a_bytes(kkr, sizeof(kkr)));
    indcpa_enc(ct, kbuf, pk, kkr + WEAVER_SSBYTES);
    trace_line("replay_indcpa_ct", fnv1a_bytes(ct, sizeof(ct)));

    crypto_kem_enc_derand(ct, ss, pk, replay_enc);
    trace_line("kem_ct_replaycoins", fnv1a_bytes(ct, sizeof(ct)));
  }

  indcpa_keypair_derand(pk, sk, kp_coins);
  trace_line("pk", fnv1a_bytes(pk, WEAVER_PUBLICKEYBYTES));

  memcpy(buf, enc_coins, WEAVER_SYMBYTES);
  memset(buf + WEAVER_SYMBYTES, 0, WEAVER_INDCPA_MSGBYTES - WEAVER_SYMBYTES);
  hash_h(buf + WEAVER_SYMBYTES, pk, WEAVER_PUBLICKEYBYTES);
  shake256(kr, sizeof(kr), buf, sizeof(buf));

  trace_encap(buf, pk, kr + WEAVER_SSBYTES);

  {
    uint8_t ct[WEAVER_CIPHERTEXTBYTES];
    uint8_t ss[WEAVER_SSBYTES];
    uint8_t enc_in[WEAVER_INDCPA_MSGBYTES];

    for(i = 0; i < WEAVER_SYMBYTES; i++)
      enc_in[i] = enc_coins[i];
    memset(enc_in + WEAVER_SYMBYTES, 0, WEAVER_INDCPA_MSGBYTES - WEAVER_SYMBYTES);

    crypto_kem_enc_derand(ct, ss, pk, enc_in);
    trace_line("kem_ct_padded", fnv1a_bytes(ct, sizeof(ct)));

    /* Same bug as kem_derand_replay: only 32-byte buffer on stack before pk */
    {
      uint8_t enc_short[WEAVER_SYMBYTES];
      uint8_t pk2[WEAVER_PUBLICKEYBYTES];
      uint8_t sk2[WEAVER_SECRETKEYBYTES];
      memcpy(pk2, pk, sizeof(pk2));
      memcpy(sk2, sk, sizeof(sk2));
      for(i = 0; i < WEAVER_SYMBYTES; i++)
        enc_short[i] = enc_coins[i];
      crypto_kem_enc_derand(ct, ss, pk2, enc_short);
      trace_line("kem_ct_shortbuf", fnv1a_bytes(ct, sizeof(ct)));
    }
  }
  return 0;
}
