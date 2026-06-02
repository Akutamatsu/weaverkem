#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "params.h"
#include "poly.h"

/* Compress_q(x,d) = round(2^d/q * x) mod 2^d */
static uint16_t compress_q(int16_t x, int d)
{
  uint32_t mask = (1u << d) - 1;
  return (uint16_t)(((((uint32_t)(uint16_t)x << d) + WEAVER_Q/2) / WEAVER_Q) & mask);
}

/* Decompress_q(y,d) = round(q/2^d * y) */
static int16_t decompress_q(uint16_t y, int d)
{
  return (int16_t)(((uint32_t)y * WEAVER_Q + (1u << (d-1))) >> d);
}

/* Pack n d-bit values into bytes */
static void pack_bits(uint8_t *out, const uint16_t *vals, int n, int d)
{
  int bits = 0, buf = 0, idx = 0, i;
  for(i = 0; i < n; i++) {
    buf |= (vals[i] & ((1<<d)-1)) << bits;
    bits += d;
    while(bits >= 8) {
      out[idx++] = (uint8_t)(buf & 0xFF);
      buf >>= 8; bits -= 8;
    }
  }
  if(bits > 0) out[idx] = (uint8_t)(buf & 0xFF);
}

/* Unpack bytes into n d-bit values */
static void unpack_bits(uint16_t *vals, const uint8_t *in, int n, int d)
{
  int bits = 0, buf = 0, idx = 0, i;
  uint16_t mask = (uint16_t)((1<<d)-1);
  for(i = 0; i < n; i++) {
    while(bits < d) { buf |= (int)in[idx++] << bits; bits += 8; }
    vals[i] = (uint16_t)(buf & mask);
    buf >>= d; bits -= d;
  }
}

typedef struct { const char *name; int n, dv, du, dt; } compress_param;

static int test_one(const compress_param *p)
{
  int i, passed = 0;
  int allowed_v = WEAVER_Q / (1 << (p->dv+1)) + 1;
  int allowed_u = WEAVER_Q / (1 << (p->du+1)) + 1;
  uint16_t comp[512], unp[512];
  uint8_t  packed[768];
  int32_t  max_err;

  printf("\n--- %s (n=%d, dv=%d, du=%d, dt=%d) ---\n",
         p->name, p->n, p->dv, p->du, p->dt);
  printf("  Poly bytes (12-bit): %d  Compressed v: %d bytes  Compressed u: %d bytes\n",
         p->n*12/8, p->n*p->dv/8, p->n*p->du/8);

  /* Test A: v -- compress -> pack -> unpack -> decompress */
  printf("  [A] v (dv=%d): ", p->dv);
  for(i=0;i<p->n;i++) comp[i] = compress_q((int16_t)((i*137+23)%WEAVER_Q), p->dv);
  memset(packed,0,sizeof(packed));
  pack_bits(packed, comp, p->n, p->dv);
  unpack_bits(unp, packed, p->n, p->dv);
  /* check pack/unpack */
  { int ok=1; for(i=0;i<p->n;i++) if(comp[i]!=unp[i]){ok=0;break;}
    if(!ok){printf("pack/unpack FAILED\n"); return 0;} }
  /* check decompress error */
  max_err=0;
  for(i=0;i<p->n;i++){
    int16_t orig=(int16_t)((i*137+23)%WEAVER_Q);
    int32_t err=(int32_t)orig-(int32_t)decompress_q(unp[i],p->dv);
    if(err<0)err=-err; if(err>WEAVER_Q/2)err=WEAVER_Q-err;
    if(err>max_err)max_err=err;
  }
  printf("max_err=%d allowed=%d %s\n", max_err, allowed_v,
         max_err<=allowed_v?"PASSED":"FAILED");
  if(max_err<=allowed_v) passed++;

  /* Show first 4 examples */
  printf("  Example (first 4 coeffs for dv=%d):\n", p->dv);
  for(i=0;i<4;i++){
    int16_t orig=(int16_t)((i*137+23)%WEAVER_Q);
    uint16_t c=compress_q(orig,p->dv);
    int16_t  d2=decompress_q(c,p->dv);
    int32_t  err=(int32_t)orig-(int32_t)d2;
    if(err<0)err=-err; if(err>WEAVER_Q/2)err=WEAVER_Q-err;
    printf("    orig=%4d -> compress(%d-bit)=%3d -> decompress=%4d err=%d\n",
           orig, p->dv, c, d2, err);
  }

  /* Test B: u */
  printf("  [B] u (du=%d): ", p->du);
  for(i=0;i<p->n;i++) comp[i]=compress_q((int16_t)((i*211+57)%WEAVER_Q),p->du);
  memset(packed,0,sizeof(packed));
  pack_bits(packed,comp,p->n,p->du);
  unpack_bits(unp,packed,p->n,p->du);
  { int ok=1; for(i=0;i<p->n;i++) if(comp[i]!=unp[i]){ok=0;break;}
    if(!ok){printf("pack/unpack FAILED\n"); return 0;} }
  max_err=0;
  for(i=0;i<p->n;i++){
    int16_t orig=(int16_t)((i*211+57)%WEAVER_Q);
    int32_t err=(int32_t)orig-(int32_t)decompress_q(unp[i],p->du);
    if(err<0)err=-err; if(err>WEAVER_Q/2)err=WEAVER_Q-err;
    if(err>max_err)max_err=err;
  }
  printf("max_err=%d allowed=%d %s\n", max_err, allowed_u,
         max_err<=allowed_u?"PASSED":"FAILED");
  if(max_err<=allowed_u) passed++;

  /* Test C: boundary */
  printf("  [C] Boundary (zero and q-1): ");
  { int ok=1;
    if(decompress_q(compress_q(0,p->dv),p->dv)!=0) ok=0;
    int32_t err=(int32_t)(WEAVER_Q-1)-(int32_t)decompress_q(compress_q(WEAVER_Q-1,p->dv),p->dv);
    if(err<0)err=-err; if(err>WEAVER_Q/2)err=WEAVER_Q-err;
    if(err>allowed_v) ok=0;
    printf("%s\n", ok?"PASSED":"FAILED");
    if(ok) passed++;
  }

  /* Test D: idempotency */
  printf("  [D] Idempotency compress(decompress(compress(x)))==compress(x): ");
  { int ok=1;
    for(i=0;i<p->n;i++){
      int16_t orig=(int16_t)((i*97+31)%WEAVER_Q);
      uint16_t c1=compress_q(orig,p->dv);
      uint16_t c2=compress_q(decompress_q(c1,p->dv),p->dv);
      if(c1!=c2){ok=0;break;}
    }
    printf("%s\n", ok?"PASSED":"FAILED");
    if(ok) passed++;
  }

  printf("  Result: %d/4 passed\n", passed);
  return passed==4;
}

static int test_actual(void)
{
  poly orig, recov;
  uint8_t comp[WEAVER_POLYCOMPRESSEDBYTES];
  int i; int32_t max_err=0;
#if   (WEAVER_POLYCOMPRESSEDBYTES==(WEAVER_N*4/8))
  int dv=4;
#elif (WEAVER_POLYCOMPRESSEDBYTES==(WEAVER_N*5/8))
  int dv=5;
#elif (WEAVER_POLYCOMPRESSEDBYTES==(WEAVER_N*6/8))
  int dv=6;
#else
  int dv=4;
#endif
  int allowed=WEAVER_Q/(1<<(dv+1))+1;

  printf("\n--- Actual poly_compress/poly_decompress (MODE=%d, N=%d, dv=%d) ---\n",
         WEAVER_MODE, WEAVER_N, dv);

  for(i=0;i<WEAVER_N;i++) orig.coeffs[i]=(int16_t)((i*137+23)%WEAVER_Q);
  poly_compress(comp, &orig);

  printf("  Compressed bytes (first 8): ");
  for(i=0;i<8&&i<WEAVER_POLYCOMPRESSEDBYTES;i++) printf("%02X ",comp[i]);
  printf("\n");

  poly_decompress(&recov, comp);

  printf("  Example (first 4 coeffs):\n");
  for(i=0;i<4;i++){
    int32_t err=(int32_t)orig.coeffs[i]-(int32_t)recov.coeffs[i];
    if(err<0)err=-err; if(err>WEAVER_Q/2)err=WEAVER_Q-err;
    printf("    orig=%4d -> decomp=%4d err=%d\n",orig.coeffs[i],recov.coeffs[i],err);
  }

  for(i=0;i<WEAVER_N;i++){
    int32_t err=(int32_t)orig.coeffs[i]-(int32_t)recov.coeffs[i];
    if(err<0)err=-err; if(err>WEAVER_Q/2)err=WEAVER_Q-err;
    if(err>max_err)max_err=err;
  }
  printf("  poly_compress->poly_decompress: max_err=%d allowed=%d %s\n",
         max_err, allowed, max_err<=allowed?"PASSED":"FAILED");
  return max_err<=allowed;
}

int main(void)
{
  int tp=0, ts=0, s;
  compress_param sets[3]={
    {"WEAVER-512",  256, 4, 8, 8},
    {"WEAVER-1024", 256, 4, 9, 9},
    {"WEAVER-2048", 512, 6, 9, 9}
  };

  printf("==============================================\n");
  printf("  Compress/Decompress Tests (All 3 Param Sets)\n");
  printf("  WEAVER_Q = %d\n", WEAVER_Q);
  printf("  Formula: Compress(x,d)=round(2^d/q*x) mod 2^d\n");
  printf("           Decompress(y,d)=round(q/2^d*y)\n");
  printf("==============================================\n");

  for(s=0;s<3;s++){
    if(test_one(&sets[s])) tp++;
    ts++;
  }

  printf("\n==============================================\n");
  printf("  Actual Implementation Test\n");
  printf("==============================================\n");
  if(test_actual()) tp++;
  ts++;

  printf("\n==============================================\n");
  printf("  Final: %d / %d passed\n", tp, ts);
  printf("==============================================\n");
  return tp==ts ? 0 : 1;
}
