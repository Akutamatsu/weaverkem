/* Standalone regression test; link with the mode-7 library sources (no bench).
 * AVX2 results are checked against scalar NTT/product oracles, exact
 * compression formulas, and a deterministic KEM transcript.
 * No randombytes implementation is needed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "params.h"
#include "poly.h"
#include "polyvec.h"
#include "msgenc.h"
#include "kem.h"
#include "indcpa.h"
#include "ntt.h"
#include "reduce.h"
#include "fips202.h"
#include "randombytes.h"
#include "invq_table_d10.h"

#if WEAVER_MODE != 7
#error "This regression test requires WEAVER_MODE=7"
#endif

void randombytes(uint8_t *out, size_t len)
{
  (void)out; (void)len;
  abort(); /* All KEM calls below use explicit deterministic coins. */
}

#define CHECK(x) do { if(!(x)) { \
  fprintf(stderr, "FAIL at line %d: %s\n", __LINE__, #x); return 1; \
} } while(0)

static int modq(int x) { x %= KYBER_Q; return x < 0 ? x + KYBER_Q : x; }
static int equal_modq(const poly *a, const poly *b)
{
  for(unsigned i=0;i<KYBER_N;i++)
    if(modq(a->coeffs[i]) != modq(b->coeffs[i])) return 0;
  return 1;
}

static void pack_bits(uint8_t *out, const poly *p, unsigned d)
{
  unsigned acc=0, bits=0;
  for(unsigned i=0;i<KYBER_N;i++) {
    unsigned x=modq(p->coeffs[i]);
    unsigned y=(((x<<d)+KYBER_Q/2)/KYBER_Q)&((1u<<d)-1);
    acc |= y<<bits; bits+=d;
    while(bits>=8) { *out++=(uint8_t)acc; acc>>=8; bits-=8; }
  }
}

int main(void)
{
  poly a,b,c,expected,orig;
  polyvec va,vb,decoded;
  uint8_t packed[KYBER_PK_POLYVECBYTES], gold[KYBER_PK_POLYVECBYTES];
  uint8_t vbytes[KYBER_POLYCOMPRESSEDBYTES];
  uint8_t pk[KYBER_PUBLICKEYBYTES],sk[KYBER_SECRETKEYBYTES];
  uint8_t ct[KYBER_CIPHERTEXTBYTES],ss[KYBER_SSBYTES],ss2[KYBER_SSBYTES];
  uint8_t coins[64],msg[KYBER_INDCPA_MSGBYTES],out[KYBER_INDCPA_MSGBYTES];
  keccak_state transcript;
  uint8_t digest[32];
  shake256_init(&transcript);

  for(unsigned y=0;y<1024;y++)
    for(unsigned j=0;j<invq_d10_bucket_size[y];j++) {
      unsigned x=(invq_d10_bucket_lo[y]+j)%KYBER_Q;
      CHECK(((((x<<10)+KYBER_Q/2)/KYBER_Q)&1023)==y);
    }

  /* Exhaust every residue, both positive and centered representatives. */
  for(unsigned base=0;base<2*KYBER_Q;base+=KYBER_N) {
    for(unsigned i=0;i<KYBER_N;i++) {
      int x=(base+i)%KYBER_Q;
      if(base+i>=KYBER_Q && x>KYBER_Q/2) x-=KYBER_Q;
      a.coeffs[i]=(int16_t)x;
    }
    for(unsigned j=0;j<KYBER_K;j++) va.vec[j]=a;
    polyvec_compress_pk(packed,&va);
    for(unsigned j=0;j<KYBER_K;j++) pack_bits(gold+j*640,&a,10);
    CHECK(!memcmp(packed,gold,sizeof packed));
    polyvec_decompress_pk(&decoded,packed);
    for(unsigned i=0;i<KYBER_N;i++) {
      unsigned t=(((unsigned)modq(a.coeffs[i])<<10)+KYBER_Q/2)/KYBER_Q;
      CHECK(decoded.vec[0].coeffs[i]==(int)(((t&1023)*KYBER_Q+512)>>10));
    }
    poly_compress(vbytes,&a); pack_bits(gold,&a,6);
    CHECK(!memcmp(vbytes,gold,sizeof vbytes));
    poly_decompress(&b,vbytes);
    for(unsigned i=0;i<KYBER_N;i++) {
      unsigned t=(((unsigned)modq(a.coeffs[i])<<6)+KYBER_Q/2)/KYBER_Q;
      CHECK(b.coeffs[i]==(int)(((t&63)*KYBER_Q+32)>>6));
    }
  }

  for(unsigned round=0;round<1000;round++) {
    shake256(coins,sizeof coins,(const uint8_t *)&round,sizeof round);
    shake256(msg,sizeof msg,coins,sizeof coins);
    for(unsigned i=0;i<KYBER_N;i++) {
      a.coeffs[i]=(int16_t)((coins[i%64]*13+i*7)%KYBER_Q);
      b.coeffs[i]=(int16_t)((msg[i%64]*11+i*3)%KYBER_Q);
    }
    orig=a; expected=a; ntt(expected.coeffs); poly_ntt(&a);
    CHECK(equal_modq(&a,&expected));
    poly_invntt_tomont(&a);
    for(unsigned i=0;i<KYBER_N;i++) a.coeffs[i]=montgomery_reduce(a.coeffs[i]);
    CHECK(equal_modq(&a,&orig));
    a=expected; poly_ntt(&b);
    for(unsigned i=0;i<KYBER_N/8;i++) {
      basemul_degree4(expected.coeffs+8*i,a.coeffs+8*i,b.coeffs+8*i,zetas[64+i]);
      basemul_degree4(expected.coeffs+8*i+4,a.coeffs+8*i+4,b.coeffs+8*i+4,-zetas[64+i]);
    }
    poly_basemul_montgomery(&c,&a,&b); CHECK(equal_modq(&c,&expected));
    for(unsigned j=0;j<KYBER_K;j++) { va.vec[j]=a; vb.vec[j]=b; }
    polyvec_basemul_acc_montgomery(&c,&va,&vb);
    for(unsigned i=0;i<KYBER_N;i++) expected.coeffs[i]*=KYBER_K;
    CHECK(equal_modq(&c,&expected));
    poly_frommsg(&a,msg); poly_tomsg(out,&a);
    CHECK(!memcmp(msg,out,sizeof msg));
    /* Seven high-bit errors, at distinct positions. */
    for(unsigned i=0;i<7;i++) a.coeffs[i*17]=modq(a.coeffs[i*17]+KYBER_Q/2);
    poly_tomsg(out,&a); CHECK(!memcmp(msg,out,sizeof msg));
    crypto_kem_keypair_derand(pk,sk,coins);
    crypto_kem_enc_derand(ct,ss,pk,msg); crypto_kem_dec(ss2,ct,sk);
    CHECK(!memcmp(ss,ss2,sizeof ss));
    shake256_absorb(&transcript,pk,sizeof pk);
    shake256_absorb(&transcript,sk,sizeof sk);
    shake256_absorb(&transcript,ct,sizeof ct);
    shake256_absorb(&transcript,ss,sizeof ss);
    ct[round%sizeof ct]^=1; crypto_kem_dec(ss2,ct,sk);
    CHECK(memcmp(ss,ss2,sizeof ss)!=0);
  }
  shake256_finalize(&transcript); shake256_squeeze(digest,sizeof digest,&transcript);
  printf("mode 7: NTT, products, codec, compression, lifting, 1000 KEM rounds and rejection OK\n");
  printf("transcript: "); for(unsigned i=0;i<sizeof digest;i++) printf("%02x",digest[i]);
  puts(""); return 0;
}
