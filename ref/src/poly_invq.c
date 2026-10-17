/**
 * poly_invq.c — WEAVER-Inv: Randomized Lifting via Inv_q (8-bit Edition)
 *
 * 修复点：
 * 1) y=0 桶使用模 q 连续区间起点（环绕起点）
 * 2) 使用 bit masking 做拒绝采样
 */

#include <stdint.h>
#include <string.h>
#include "params.h"
#include "poly.h"
#include "invq.h"
#include "symmetric.h"

#if (KYBER_PK_POLYVECBYTES == (KYBER_K * KYBER_N * 10 / 8))
#include "invq_table_d10.h"
#define BUCKET_LO invq_d10_bucket_lo
#define BUCKET_SZ invq_d10_bucket_size
#elif (KYBER_PK_POLYVECBYTES == (KYBER_K * KYBER_N * 9 / 8))
#include "invq_table_d9.h"
#define BUCKET_LO invq_d9_bucket_lo
#define BUCKET_SZ invq_d9_bucket_size
#else
#error "Inv_q table not available for this compression width"
#endif

static unsigned int rej_uniform(int16_t *r,
    unsigned int len,
    const uint8_t *buf,
    unsigned int buflen)
{
    unsigned int ctr, pos, j;
    uint8_t t[8];

    ctr = pos = 0;
    while (ctr < len && pos + 3 <= buflen) { // 3 bytes -> 8 x 3 bits
        t[0] = (buf[pos + 0] >> 0) & 0x7;
        t[1] = (buf[pos + 0] >> 3) & 0x7;
        t[2] = ((buf[pos + 0] >> 6) | (buf[pos + 1] << 2)) & 0x7;
        t[3] = (buf[pos + 1] >> 1) & 0x7;
        t[4] = (buf[pos + 1] >> 4) & 0x7;
        t[5] = ((buf[pos + 1] >> 7) | (buf[pos + 2] << 1)) & 0x7;
        t[6] = (buf[pos + 2] >> 2) & 0x7;
        t[7] = (buf[pos + 2] >> 5) & 0x7;
        pos += 3;

        for (j = 0; j < 8; j++) {
            if (ctr >= len) {
                break;
            }
            if (t[j] < BUCKET_SZ[r[ctr]]) {
                unsigned int c = ctr;
                int16_t bidx = r[c];
                r[c] = BUCKET_LO[bidx] + t[j];
                ctr = c + 1;
            }
        }
    }

    return ctr;
}

/* where (6/7) is the rejection rate (upper bound) */
//#define GEN_INVQ_RAND_BYTES (3 *KYBER_N/8 * (7/6))

#if KYBER_N == 256
#define GEN_INVQ_RAND_BYTES SHAKE256_RATE
#elif KYBER_N == 512
#define GEN_INVQ_RAND_BYTES (2*SHAKE256_RATE)
#endif

//void poly_invq(poly *r, const uint8_t seed[KYBER_SYMBYTES], uint8_t nonce)
//{
//    uint8_t buf[GEN_INVQ_RAND_BYTES];
//    unsigned int buflen = GEN_INVQ_RAND_BYTES;
//    prf(buf, sizeof(buf), seed, nonce);
//    rej_uniform(r->coeffs, KYBER_N, buf, buflen);
//}

void polyvec_invq(polyvec *v,
                  const uint8_t seed[KYBER_SYMBYTES],
                  uint8_t nonce)
{
    unsigned int ctr, i;
    unsigned int buflen;
    uint8_t buf[GEN_INVQ_RAND_BYTES];

    for (i = 0; i < KYBER_K; i++) {
        prf(buf, GEN_INVQ_RAND_BYTES, seed, nonce);
        buflen = GEN_INVQ_RAND_BYTES;
        ctr = rej_uniform(v->vec[i].coeffs, KYBER_N, buf, buflen);

        while (ctr < KYBER_N) {
            prf(buf, GEN_INVQ_RAND_BYTES, seed, nonce);
            buflen = GEN_INVQ_RAND_BYTES;
            ctr += rej_uniform(v->vec[i].coeffs + ctr, KYBER_N - ctr, buf, buflen);
        }
    }
}
