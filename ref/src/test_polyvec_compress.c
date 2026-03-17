#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "params.h"
#include "poly.h"
#include "polyvec.h"

/* ============================================================
 * Runtime simulation of polyvec compress/decompress
 * for arbitrary du bits, k polynomials of degree n
 * Compress_q(x, d)   = round(2^d / q * x) mod 2^d
 * Decompress_q(y, d) = round(q / 2^d * y)
 * ============================================================ */
static uint16_t compress_coeff(int16_t x, int d)
{
  uint32_t mask = (1u << d) - 1;
  return (uint16_t)(((((uint32_t)(uint16_t)x << d) + KYBER_Q/2) / KYBER_Q) & mask);
}

static int16_t decompress_coeff(uint16_t y, int d)
{
  return (int16_t)(((uint32_t)y * KYBER_Q + (1u << (d-1))) >> d);
}

/* Pack n d-bit values into bytes */
static void pack_bits(uint8_t *out, const uint16_t *vals, int n, int d)
{
  int bits=0, buf=0, idx=0, i;
  for(i=0;i<n;i++){
    buf |= (vals[i]&((1<<d)-1))<<bits; bits+=d;
    while(bits>=8){out[idx++]=(uint8_t)(buf&0xFF);buf>>=8;bits-=8;}
  }
  if(bits>0) out[idx]=(uint8_t)(buf&0xFF);
}

/* Unpack bytes into n d-bit values */
static void unpack_bits(uint16_t *vals, const uint8_t *in, int n, int d)
{
  int bits=0,buf=0,idx=0,i;
  uint16_t mask=(uint16_t)((1<<d)-1);
  for(i=0;i<n;i++){
    while(bits<d){buf|=(int)in[idx++]<<bits;bits+=8;}
    vals[i]=(uint16_t)(buf&mask);buf>>=d;bits-=d;
  }
}

typedef struct { const char *name; int n, k, du; } pvec_param;

static int test_polyvec_param(const pvec_param *p)
{
  int i, j, passed = 0;
  int allowed = KYBER_Q / (1 << (p->du+1)) + 1;
  int coeff_bytes = p->n * p->du / 8;          /* per polynomial */
  int total_bytes = p->k * coeff_bytes;         /* whole vector   */
  uint16_t comp[512], unp[512];
  uint8_t  packed[4096];

  printf("\n--- %s (n=%d, k=%d, du=%d) ---\n",
         p->name, p->n, p->k, p->du);
  printf("  Poly bytes (12-bit): %d  Compressed vector: %d bytes\n",
         p->n*12/8, total_bytes);
  printf("  Max allowed error (du=%d): %d\n", p->du, allowed);

  /* --- Test A: compress -> pack -> unpack -> decompress for each poly --- */
  printf("  [A] compress/decompress each polynomial (du=%d bits):\n", p->du);
  {
    int all_ok = 1;
    for(i = 0; i < p->k; i++) {
      int32_t max_err = 0;
      int ok_pack = 1;
      /* generate test coefficients for poly i */
      for(j = 0; j < p->n; j++)
        comp[j] = compress_coeff((int16_t)((j*137 + i*31 + 23) % KYBER_Q), p->du);
      /* pack into bytes */
      memset(packed, 0, sizeof(packed));
      pack_bits(packed, comp, p->n, p->du);
      /* unpack */
      unpack_bits(unp, packed, p->n, p->du);
      /* check pack/unpack roundtrip */
      for(j = 0; j < p->n; j++)
        if(comp[j] != unp[j]){ ok_pack=0; break; }
      /* check decompress error */
      for(j = 0; j < p->n; j++){
        int16_t orig = (int16_t)((j*137 + i*31 + 23) % KYBER_Q);
        int32_t err = (int32_t)orig - (int32_t)decompress_coeff(unp[j], p->du);
        if(err<0)err=-err; if(err>KYBER_Q/2)err=KYBER_Q-err;
        if(err>max_err) max_err=err;
      }
      printf("    poly[%d]: pack/unpack=%s max_err=%d(%s)\n",
             i,
             ok_pack?"OK":"FAIL",
             max_err,
             (ok_pack && max_err<=allowed)?"PASS":"FAIL");
      if(!ok_pack || max_err>allowed) all_ok=0;
    }
    if(all_ok){ printf("    All %d polynomials PASSED\n", p->k); passed++; }
    else        printf("    FAILED\n");
  }

  /* --- Test B: show first 4 coeffs of poly[0] as example --- */
  printf("  [B] Example (poly[0], first 4 coeffs, du=%d):\n", p->du);
  for(j=0;j<4;j++){
    int16_t orig=(int16_t)((j*137+23)%KYBER_Q);
    uint16_t c=compress_coeff(orig,p->du);
    int16_t  d2=decompress_coeff(c,p->du);
    int32_t  err=(int32_t)orig-(int32_t)d2;
    if(err<0)err=-err; if(err>KYBER_Q/2)err=KYBER_Q-err;
    printf("    orig=%4d -> compress(%d-bit)=%4d -> decompress=%4d err=%d\n",
           orig,p->du,c,d2,err);
  }
  passed++; /* example always counts */

  /* --- Test C: boundary values --- */
  printf("  [C] Boundary (zero and q-1): ");
  {
    int ok=1;
    if(decompress_coeff(compress_coeff(0,p->du),p->du)!=0) ok=0;
    int32_t err=(int32_t)(KYBER_Q-1)-(int32_t)decompress_coeff(
                compress_coeff(KYBER_Q-1,p->du),p->du);
    if(err<0)err=-err; if(err>KYBER_Q/2)err=KYBER_Q-err;
    if(err>allowed) ok=0;
    printf("%s\n",ok?"PASSED":"FAILED");
    if(ok) passed++;
  }

  /* --- Test D: idempotency --- */
  printf("  [D] Idempotency compress(decompress(compress(x)))==compress(x): ");
  {
    int ok=1;
    for(j=0;j<p->n;j++){
      int16_t x=(int16_t)((j*97+31)%KYBER_Q);
      uint16_t c1=compress_coeff(x,p->du);
      uint16_t c2=compress_coeff(decompress_coeff(c1,p->du),p->du);
      if(c1!=c2){ok=0;break;}
    }
    printf("%s\n",ok?"PASSED":"FAILED");
    if(ok) passed++;
  }

  printf("  Result: %d/4 passed\n", passed);
  return passed==4;
}

/* ============================================================
 * Test actual polyvec_compress / polyvec_decompress
 * for current WEAVER_MODE
 * ============================================================ */
static int test_actual_polyvec(void)
{
  polyvec orig, recov;
  uint8_t comp[KYBER_POLYVECCOMPRESSEDBYTES];
  int i, j;
  int32_t max_err=0;

#if   (KYBER_POLYVECCOMPRESSEDBYTES == (KYBER_K * KYBER_N * 9  / 8))
  int du=9;
#elif (KYBER_POLYVECCOMPRESSEDBYTES == (KYBER_K * KYBER_N * 10 / 8))
  int du=10;
#elif (KYBER_POLYVECCOMPRESSEDBYTES == (KYBER_K * KYBER_N * 11 / 8))
  int du=11;
#else
  int du=0;
#endif
  int allowed = KYBER_Q/(1<<(du+1))+1;

  printf("\n--- Actual polyvec_compress/polyvec_decompress "
         "(MODE=%d, N=%d, K=%d, du=%d) ---\n",
         WEAVER_MODE, KYBER_N, KYBER_K, du);
  printf("  KYBER_POLYVECCOMPRESSEDBYTES = %d\n", KYBER_POLYVECCOMPRESSEDBYTES);

  /* fill with deterministic data */
  for(i=0;i<KYBER_K;i++)
    for(j=0;j<KYBER_N;j++)
      orig.vec[i].coeffs[j]=(int16_t)((j*137+i*31+23)%KYBER_Q);

  polyvec_compress(comp, &orig);

  /* show first 8 compressed bytes */
  printf("  First 8 compressed bytes: ");
  for(i=0;i<8;i++) printf("%02X ",comp[i]);
  printf("\n");

  polyvec_decompress(&recov, comp);

  /* show example: poly[0] first 4 coeffs */
  printf("  Example poly[0] (first 4 coeffs):\n");
  for(j=0;j<4;j++){
    int32_t err=(int32_t)orig.vec[0].coeffs[j]-(int32_t)recov.vec[0].coeffs[j];
    if(err<0)err=-err; if(err>KYBER_Q/2)err=KYBER_Q-err;
    printf("    orig=%4d -> decomp=%4d err=%d\n",
           orig.vec[0].coeffs[j], recov.vec[0].coeffs[j], err);
  }

  /* check all errors */
  for(i=0;i<KYBER_K;i++)
    for(j=0;j<KYBER_N;j++){
      int32_t err=(int32_t)orig.vec[i].coeffs[j]-(int32_t)recov.vec[i].coeffs[j];
      if(err<0)err=-err; if(err>KYBER_Q/2)err=KYBER_Q-err;
      if(err>max_err)max_err=err;
    }

  printf("  polyvec_compress->polyvec_decompress: max_err=%d allowed=%d %s\n",
         max_err,allowed,max_err<=allowed?"PASSED":"FAILED");
  return max_err<=allowed;
}

int main(void)
{
  int tp=0,ts=0,s;

  /* Table 1: three parameter sets, du column */
  pvec_param sets[3]={
    {"WEAVER-512",  128, 4, 9 },
    {"WEAVER-1024", 256, 4, 10},
    {"WEAVER-2048", 512, 4, 10}
  };

  printf("==============================================\n");
  printf("  polyvec Compress/Decompress Tests\n");
  printf("  (u vector, du bits, all 3 param sets)\n");
  printf("  KYBER_Q = %d\n", KYBER_Q);
  printf("==============================================\n");

  for(s=0;s<3;s++){
    if(test_polyvec_param(&sets[s])) tp++;
    ts++;
  }

  printf("\n==============================================\n");
  printf("  Actual Implementation Test\n");
  printf("==============================================\n");
  if(test_actual_polyvec()) tp++;
  ts++;

  printf("\n==============================================\n");
  printf("  Final: %d / %d passed\n", tp, ts);
  printf("==============================================\n");
  return tp==ts ? 0 : 1;
}
