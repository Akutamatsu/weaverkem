#include <stdint.h>
#include "params.h"
#include "polyvec.h"
#include "invq.h"
#include "symmetric.h"

#if (WEAVER_PK_POLYVECBYTES == (WEAVER_K * WEAVER_N * 8 / 8))
#define WEAVER_DT 8
#elif (WEAVER_PK_POLYVECBYTES == (WEAVER_K * WEAVER_N * 9 / 8))
#define WEAVER_DT 9
#elif (WEAVER_PK_POLYVECBYTES == (WEAVER_K * WEAVER_N * 10 / 8))
#define WEAVER_DT 10
#elif (WEAVER_PK_POLYVECBYTES == (WEAVER_K * WEAVER_N * 11 / 8))
#define WEAVER_DT 11
#else
#error "Unsupported public-key compression width"
#endif

#define INVQ_BUCKETS (1u << WEAVER_DT)
#define INVQ_RAND_MASK ((WEAVER_Q <= 4096) ? 0x0FFFu : 0xFFFFu)
#if WEAVER_N == 512
#define GEN_INVQ_RAND_BYTES (2*SHAKE256_RATE)
#else
#define GEN_INVQ_RAND_BYTES SHAKE256_RATE
#endif

static uint16_t compress_q(uint16_t x)
{
  return (uint16_t)(((((uint32_t)x << WEAVER_DT) + WEAVER_Q / 2) / WEAVER_Q) & (INVQ_BUCKETS - 1));
}

static unsigned int rej_uniform_modq(uint16_t *r,
                                     unsigned int len,
                                     const uint8_t *buf,
                                     unsigned int buflen)
{
  unsigned int ctr = 0;
  unsigned int pos = 0;

  while(ctr < len && pos + 3 <= buflen) {
    uint16_t val0 = ((buf[pos + 0] >> 0) | ((uint16_t)buf[pos + 1] << 8)) & INVQ_RAND_MASK;
    uint16_t val1 = ((buf[pos + 1] >> 4) | ((uint16_t)buf[pos + 2] << 4)) & INVQ_RAND_MASK;
    pos += 3;

    if(val0 < WEAVER_Q)
      r[ctr++] = val0;
    if(ctr < len && val1 < WEAVER_Q)
      r[ctr++] = val1;
  }

  return ctr;
}

static void invq_poly(poly *r,
                      const poly *bucket,
                      const uint8_t seed[WEAVER_SYMBYTES],
                      uint8_t nonce)
{
  unsigned int i, ctr;
  uint8_t buf[GEN_INVQ_RAND_BYTES];
  uint16_t candidates[WEAVER_N];

  ctr = 0;
  while(ctr < WEAVER_N) {
    prf(buf, sizeof(buf), seed, nonce++);
    ctr += rej_uniform_modq(candidates + ctr,
                            WEAVER_N - ctr,
                            buf,
                            sizeof(buf));
  }

  for(i = 0; i < WEAVER_N; i++) {
    uint16_t y = (uint16_t)bucket->coeffs[i] & (INVQ_BUCKETS - 1);
    uint16_t x = candidates[i];

    while(compress_q(x) != y) {
      x++;
      if(x == WEAVER_Q)
        x = 0;
    }

    r->coeffs[i] = (int16_t)x;
  }
}

void polyvec_invq(polyvec *v,
                  const uint8_t seed[WEAVER_SYMBYTES],
                  uint8_t nonce)
{
  unsigned int i;

  for(i = 0; i < WEAVER_K; i++)
    invq_poly(&v->vec[i], &v->vec[i], seed, nonce++);
}
