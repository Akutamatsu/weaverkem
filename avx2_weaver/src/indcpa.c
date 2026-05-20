#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "params.h"
#include "indcpa.h"
#include "polyvec.h"
#include "poly.h"
#include "msgenc.h"
#include "ntt.h"
#include "symmetric.h"

#ifdef WEAVER_PROFILE_KEYPAIR_DERAND
#include "cpucycles.h"
#include "weaver_kp_profile.h"
#define WEAVER_KP_T0() uint64_t weaver_kp_ts = cpucycles()
#define WEAVER_KP_T(slot)                                                      \
  do {                                                                         \
    uint64_t weaver_kp_te = cpucycles();                                      \
    weaver_kp_prof_segment((slot), weaver_kp_te - weaver_kp_ts);               \
    weaver_kp_ts = weaver_kp_te;                                               \
  } while(0)
#else
#define WEAVER_KP_T0() ((void)0)
#define WEAVER_KP_T(slot) ((void)0)
#endif

#ifdef PK_COMPRESS
#include "invq.h"
#if !defined(NO_INV_Q_LIFTING)
#define INV_Q_LIFTING
#endif
#endif

/*************************************************
* Name:        pack_pk
*
* Description: Serialize the public key as concatenation of the
*              serialized vector of polynomials pk
*              and the public seed used to generate the matrix A.
*
* Arguments:   uint8_t *r:          pointer to the output serialized public key
*              polyvec *pk:         pointer to the input public-key polyvec
*              const uint8_t *seed: pointer to the input public seed
**************************************************/
static void pack_pk(uint8_t r[KYBER_INDCPA_PUBLICKEYBYTES],
                    polyvec *pk,
                    const uint8_t seed[KYBER_SYMBYTES])
{
#ifdef PK_COMPRESS
  polyvec_compress_pk(r, pk);
  memcpy(r+ KYBER_PK_POLYVECBYTES, seed, KYBER_SYMBYTES);
#else
  polyvec_tobytes(r, pk);
  memcpy(r+KYBER_POLYVECBYTES, seed, KYBER_SYMBYTES);
#endif
}

/*************************************************
* Name:        unpack_pk
*
* Description: De-serialize public key from a byte array;
*              approximate inverse of pack_pk
*
* Arguments:   - polyvec *pk:             pointer to output public-key
*                                         polynomial vector
*              - uint8_t *seed:           pointer to output seed to generate
*                                         matrix A
*              - const uint8_t *packedpk: pointer to input serialized public key
**************************************************/
void unpack_pk(polyvec *pk,
                      uint8_t seed[KYBER_SYMBYTES],
                      const uint8_t packedpk[KYBER_INDCPA_PUBLICKEYBYTES])
{
#ifdef PK_COMPRESS
  polyvec_decompress_pk(pk, packedpk);
  memcpy(seed, packedpk+KYBER_PK_POLYVECBYTES, KYBER_SYMBYTES);
#else
  polyvec_frombytes(pk, packedpk);
  memcpy(seed, packedpk+KYBER_POLYVECBYTES, KYBER_SYMBYTES);
#endif
}

/*************************************************
* Name:        pack_sk
*
* Description: Serialize the secret key
*
* Arguments:   - uint8_t *r:  pointer to output serialized secret key
*              - polyvec *sk: pointer to input vector of polynomials (secret key)
**************************************************/
static void pack_sk(uint8_t r[KYBER_INDCPA_SECRETKEYBYTES], polyvec *sk)
{
  polyvec_tobytes(r, sk);
}

/*************************************************
* Name:        unpack_sk
*
* Description: De-serialize the secret key;
*              inverse of pack_sk
*
* Arguments:   - polyvec *sk:             pointer to output vector of
*                                         polynomials (secret key)
*              - const uint8_t *packedsk: pointer to input serialized secret key
**************************************************/
static void unpack_sk(polyvec *sk,
                      const uint8_t packedsk[KYBER_INDCPA_SECRETKEYBYTES])
{
  polyvec_frombytes(sk, packedsk);
}

/*************************************************
* Name:        pack_ciphertext
*
* Description: Serialize the ciphertext as concatenation of the
*              compressed and serialized vector of polynomials b
*              and the compressed and serialized polynomial v
*
* Arguments:   uint8_t *r: pointer to the output serialized ciphertext
*              poly *pk:   pointer to the input vector of polynomials b
*              poly *v:    pointer to the input polynomial v
**************************************************/
static void pack_ciphertext(uint8_t r[KYBER_INDCPA_BYTES],
                            polyvec *b,
                            poly *v)
{
  polyvec_compress(r, b);
  poly_compress(r+KYBER_POLYVECCOMPRESSEDBYTES, v);
}

/*************************************************
* Name:        unpack_ciphertext
*
* Description: De-serialize and decompress ciphertext from a byte array;
*              approximate inverse of pack_ciphertext
*
* Arguments:   - polyvec *b:       pointer to the output vector of polynomials b
*              - poly *v:          pointer to the output polynomial v
*              - const uint8_t *c: pointer to the input serialized ciphertext
**************************************************/
static void unpack_ciphertext(polyvec *b,
                              poly *v,
                              const uint8_t c[KYBER_INDCPA_BYTES])
{
  polyvec_decompress(b, c);
  poly_decompress(v, c+KYBER_POLYVECCOMPRESSEDBYTES);
}

/*************************************************
* Name:        rej_uniform
*
* Description: Run rejection sampling on uniform random bytes to generate
*              uniform random integers mod q
*
* Arguments:   - int16_t *r:          pointer to output buffer
*              - unsigned int len:    requested number of 16-bit integers
*                                     (uniform mod q)
*              - const uint8_t *buf:  pointer to input buffer
*                                     (assumed to be uniform random bytes)
*              - unsigned int buflen: length of input buffer in bytes
*
* Returns number of sampled 16-bit integers (at most len)
**************************************************/
static unsigned int rej_uniform(int16_t *r,
                                unsigned int len,
                                const uint8_t *buf,
                                unsigned int buflen)
{
  unsigned int ctr, pos, j;
  uint16_t val0, val1;

  ctr = pos = 0;
  while(ctr < len && pos + 3 <= buflen) {
    val0 = ((buf[pos+0] >> 0) | ((uint16_t)buf[pos+1] << 8)) & 0xFFF;
    val1 = ((buf[pos+1] >> 4) | ((uint16_t)buf[pos+2] << 4)) & 0xFFF;
    pos += 3;

    if(val0 < KYBER_Q)
      r[ctr++] = val0;
    if(ctr < len && val1 < KYBER_Q)
      r[ctr++] = val1;
  }

  (void)j;
  return ctr;
}

#define gen_a(A,B)  gen_matrix(A,B,0)
#define gen_at(A,B) gen_matrix(A,B,1)

/*************************************************
* Name:        gen_matrix
*
* Description: Deterministically generate matrix A (or the transpose of A)
*              from a seed. Entries of the matrix are polynomials that look
*              uniformly random. Performs rejection sampling on output of
*              a XOF
*
* Arguments:   - polyvec *a:          pointer to ouptput matrix A
*              - const uint8_t *seed: pointer to input seed
*              - int transposed:      boolean deciding whether A or A^T
*                                     is generated
**************************************************/
#if(XOF_BLOCKBYTES % 3)
#error "Implementation of gen_matrix assumes that XOF_BLOCKBYTES is a multiple of 3"
#endif

#define GEN_MATRIX_NBLOCKS ((12*KYBER_N/8*(1 << 12)/KYBER_Q + XOF_BLOCKBYTES)/XOF_BLOCKBYTES)

#if !defined(WEAVER_AVX_GEN_MATRIX) && \
    !(defined(WEAVER_AVX_GEN_MATRIX512) && (KYBER_N == 512))
// Not static for benchmarking
void gen_matrix(polyvec *a, const uint8_t seed[KYBER_SYMBYTES], int transposed)
{
  unsigned int ctr, i, j;
  unsigned int buflen;
  uint8_t buf[GEN_MATRIX_NBLOCKS*XOF_BLOCKBYTES];
  xof_state state;

  for(i=0;i<KYBER_K;i++) {
    for(j=0;j<KYBER_K;j++) {
      if(transposed)
        xof_absorb(&state, seed, i, j);
      else
        xof_absorb(&state, seed, j, i);

      xof_squeezeblocks(buf, GEN_MATRIX_NBLOCKS, &state);
      buflen = GEN_MATRIX_NBLOCKS*XOF_BLOCKBYTES;
      ctr = rej_uniform(a[i].vec[j].coeffs, KYBER_N, buf, buflen);

      while(ctr < KYBER_N) {
        xof_squeezeblocks(buf, 1, &state);
        buflen = XOF_BLOCKBYTES;
        ctr += rej_uniform(a[i].vec[j].coeffs + ctr, KYBER_N - ctr, buf, buflen);
      }
#if defined(WEAVER_AVX256_NTT)
      /* AVX basemul expects matrix coeffs in unpacked layout (pq-crystals kyber avx2). */
      poly_nttunpack(&a[i].vec[j]);
#endif
    }
  }
}
#endif /* !WEAVER_AVX_GEN_MATRIX && !WEAVER_AVX_GEN_MATRIX512 on n=512 */

/*************************************************
* Name:        indcpa_keypair
*
* Description: Generates public and private key for the CPA-secure
*              public-key encryption scheme underlying Kyber
*
* Arguments:   - uint8_t *pk: pointer to output public key
*                             (of length KYBER_INDCPA_PUBLICKEYBYTES bytes)
*              - uint8_t *sk: pointer to output private key
                              (of length KYBER_INDCPA_SECRETKEYBYTES bytes)
**************************************************/
void indcpa_keypair_derand(uint8_t pk[KYBER_INDCPA_PUBLICKEYBYTES],
                           uint8_t sk[KYBER_INDCPA_SECRETKEYBYTES],
                           const uint8_t coins[KYBER_SYMBYTES])
{
  unsigned int i;
  uint8_t buf[2*KYBER_SYMBYTES];
  const uint8_t *publicseed = buf;
  const uint8_t *noiseseed = buf+KYBER_SYMBYTES;
  uint8_t nonce = 0;
  polyvec a[KYBER_K] = {0}, pkpv = {0}, skpv = {0};

  (void)i;

  WEAVER_KP_T0();
  memcpy(buf, coins, KYBER_SYMBYTES);
  buf[KYBER_SYMBYTES] = KYBER_K;
  hash_g(buf, buf, KYBER_SYMBYTES+1);
  WEAVER_KP_T(0); /* 0: buf + hash_g */

  gen_a(a, publicseed);
  WEAVER_KP_T(1); /* 1: gen_matrix (gen_a) */

  #if (KYBER_K == 2)
  poly_getnoise_eta1(&skpv.vec[0], noiseseed, nonce++);
  poly_getnoise_eta1(&skpv.vec[1], noiseseed, nonce++);
  #elif (KYBER_K == 4)
  poly_getnoise_eta1(&skpv.vec[0], noiseseed, nonce++);
  poly_getnoise_eta1(&skpv.vec[1], noiseseed, nonce++);
  poly_getnoise_eta1(&skpv.vec[2], noiseseed, nonce++);
  poly_getnoise_eta1(&skpv.vec[3], noiseseed, nonce++);
  #else
  for(i=0;i<KYBER_K;i++)
    poly_getnoise_eta1(&skpv.vec[i], noiseseed, nonce++);
  #endif
  WEAVER_KP_T(2); /* 2: poly_getnoise x K */

  polyvec_ntt(&skpv);
  WEAVER_KP_T(3); /* 3: polyvec_ntt (K polys) */
  
#ifndef PK_COMPRESS

  // matrix-vector multiplication
  #if (KYBER_K == 2)
  polyvec_basemul_acc_montgomery(&pkpv.vec[0], &a[0], &skpv);
  poly_tomont(&pkpv.vec[0]);
  polyvec_basemul_acc_montgomery(&pkpv.vec[1], &a[1], &skpv);
  poly_tomont(&pkpv.vec[1]);
  #elif (KYBER_K == 4)
  polyvec_basemul_acc_montgomery(&pkpv.vec[0], &a[0], &skpv);
  poly_tomont(&pkpv.vec[0]);
  polyvec_basemul_acc_montgomery(&pkpv.vec[1], &a[1], &skpv);
  poly_tomont(&pkpv.vec[1]);
  polyvec_basemul_acc_montgomery(&pkpv.vec[2], &a[2], &skpv);
  poly_tomont(&pkpv.vec[2]);
  polyvec_basemul_acc_montgomery(&pkpv.vec[3], &a[3], &skpv);
  poly_tomont(&pkpv.vec[3]);
  #else
  for(i=0;i<KYBER_K;i++) {
    polyvec_basemul_acc_montgomery(&pkpv.vec[i], &a[i], &skpv);
    poly_tomont(&pkpv.vec[i]);
  }
  #endif

  WEAVER_KP_T(4); /* matvec (NTT domain) */
  polyvec_reduce(&pkpv); /* save in NTT domain */
  WEAVER_KP_T(5); /* polyvec_reduce */
#ifdef WEAVER_PROFILE_KEYPAIR_DERAND
  weaver_kp_prof_segment(6, 0); /* no invntt in this path */
#endif

#else
  #if (KYBER_K == 2)
  polyvec_basemul_acc_montgomery(&pkpv.vec[0], &a[0], &skpv);
  polyvec_basemul_acc_montgomery(&pkpv.vec[1], &a[1], &skpv);
  #elif (KYBER_K == 4)
  polyvec_basemul_acc_montgomery(&pkpv.vec[0], &a[0], &skpv);
  polyvec_basemul_acc_montgomery(&pkpv.vec[1], &a[1], &skpv);
  polyvec_basemul_acc_montgomery(&pkpv.vec[2], &a[2], &skpv);
  polyvec_basemul_acc_montgomery(&pkpv.vec[3], &a[3], &skpv);
  #else
  for (i = 0; i < KYBER_K; i++) {
      polyvec_basemul_acc_montgomery(&pkpv.vec[i], &a[i], &skpv);
      //poly_tomont(&pkpv.vec[i]);
  }
  #endif
  WEAVER_KP_T(4); /* 4: matrix-vector (k basemul_acc), NTT domain */

  polyvec_invntt_tomont(&pkpv);  // from NTT to plain.
  WEAVER_KP_T(5); /* 5: polyvec_invntt_tomont */

  polyvec_reduce(&pkpv);
  WEAVER_KP_T(6); /* 6: polyvec_reduce */

#endif
  pack_sk(sk, &skpv);
  pack_pk(pk, &pkpv, publicseed);
  WEAVER_KP_T(7); /* 7: pack_sk + pack_pk */
}

/*************************************************
* Name:        indcpa_enc
*
* Description: Encryption function of the CPA-secure
*              public-key encryption scheme underlying Kyber.
*
* Arguments:   - uint8_t *c:           pointer to output ciphertext
*                                      (of length KYBER_INDCPA_BYTES bytes)
*              - const uint8_t *m:     pointer to input message
*                                      (of length KYBER_INDCPA_MSGBYTES bytes)
*              - const uint8_t *pk:    pointer to input public key
*                                      (of length KYBER_INDCPA_PUBLICKEYBYTES)
*              - const uint8_t *coins: pointer to input random coins
*                                      used as seed (of length KYBER_SYMBYTES)
*                                      to deterministically generate all
*                                      randomness
**************************************************/
void indcpa_enc(uint8_t c[KYBER_INDCPA_BYTES],
                const uint8_t m[KYBER_INDCPA_MSGBYTES],
                const uint8_t pk[KYBER_INDCPA_PUBLICKEYBYTES],
                const uint8_t coins[KYBER_SYMBYTES])
{
  unsigned int i;
  uint8_t seed[KYBER_SYMBYTES];
  uint8_t nonce = 0;
  polyvec sp = {0}, pkpv = {0}, at[KYBER_K] = {0}, b = {0};
  poly v = {0}, k = {0};

  (void)i;

#ifdef INV_Q_LIFTING
  /*
   * WEAVER-Inv (Algorithm 2):
   *   1. 从 pk 中提取压缩后的桶编号（不做 Decompress）
   *   2. 用 Inv_q 随机提升到 Z_q（消耗 nonce=0 的 PRF 输出）
   *   3. NTT 变换
   */
  polyvec_fromcompressed_pk(&pkpv, pk);
  memcpy(seed, pk + KYBER_PK_POLYVECBYTES, KYBER_SYMBYTES);

  polyvec_invq(&pkpv, coins, nonce++);
  polyvec_ntt(&pkpv);
#else
  unpack_pk(&pkpv, seed, pk);
#ifdef PK_COMPRESS
  polyvec_ntt(&pkpv);
#endif
#endif

  poly_frommsg(&k, m);
  gen_at(at, seed);

  #if (KYBER_K == 2)
  poly_getnoise_eta1(&sp.vec[0], coins, nonce++);
  poly_getnoise_eta1(&sp.vec[1], coins, nonce++);
  #elif (KYBER_K == 4)
  poly_getnoise_eta1(&sp.vec[0], coins, nonce++);
  poly_getnoise_eta1(&sp.vec[1], coins, nonce++);
  poly_getnoise_eta1(&sp.vec[2], coins, nonce++);
  poly_getnoise_eta1(&sp.vec[3], coins, nonce++);
  #else
  for(i=0;i<KYBER_K;i++)
    poly_getnoise_eta1(sp.vec+i, coins, nonce++);
  #endif

  polyvec_ntt(&sp);

  // matrix-vector multiplication
  #if (KYBER_K == 2)
  polyvec_basemul_acc_montgomery(&b.vec[0], &at[0], &sp);
  polyvec_basemul_acc_montgomery(&b.vec[1], &at[1], &sp);
  #elif (KYBER_K == 4)
  polyvec_basemul_acc_montgomery(&b.vec[0], &at[0], &sp);
  polyvec_basemul_acc_montgomery(&b.vec[1], &at[1], &sp);
  polyvec_basemul_acc_montgomery(&b.vec[2], &at[2], &sp);
  polyvec_basemul_acc_montgomery(&b.vec[3], &at[3], &sp);
  #else
  for(i=0;i<KYBER_K;i++)
    polyvec_basemul_acc_montgomery(&b.vec[i], &at[i], &sp);
  #endif

  polyvec_basemul_acc_montgomery(&v, &pkpv, &sp);

  polyvec_invntt_tomont(&b);
  poly_invntt_tomont(&v);

  poly_add(&v, &v, &k);
  polyvec_reduce(&b);
  poly_reduce(&v);

  pack_ciphertext(c, &b, &v);
}

/*************************************************
* Name:        indcpa_dec
*
* Description: Decryption function of the CPA-secure
*              public-key encryption scheme underlying Kyber.
*
* Arguments:   - uint8_t *m:        pointer to output decrypted message
*                                   (of length KYBER_INDCPA_MSGBYTES)
*              - const uint8_t *c:  pointer to input ciphertext
*                                   (of length KYBER_INDCPA_BYTES)
*              - const uint8_t *sk: pointer to input secret key
*                                   (of length KYBER_INDCPA_SECRETKEYBYTES)
**************************************************/
void indcpa_dec(uint8_t m[KYBER_INDCPA_MSGBYTES],
                const uint8_t c[KYBER_INDCPA_BYTES],
                const uint8_t sk[KYBER_INDCPA_SECRETKEYBYTES])
{
  polyvec b = {0}, skpv = {0};
  poly v = {0}, mp = {0};

  unpack_ciphertext(&b, &v, c);
  unpack_sk(&skpv, sk);

  polyvec_ntt(&b);
  polyvec_basemul_acc_montgomery(&mp, &skpv, &b);
  poly_invntt_tomont(&mp);

  poly_sub(&mp, &v, &mp);
  poly_reduce(&mp);

  poly_tomsg(m, &mp);
}
