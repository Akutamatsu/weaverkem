#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "params.h"
#include "poly.h"
#include "cbd.h"

static unsigned int popcount_u32(uint32_t x)
{
  unsigned int r = 0;
  while(x) {
    r += x & 1u;
    x >>= 1;
  }
  return r;
}

static void cbd_ref(int16_t *r, const uint8_t *buf, int n, int eta)
{
  int i, j;

  if(eta == 1) {
    for(i = 0; i < n; i++) {
      int16_t a = (buf[i/4] >> ((i%4)*2+0)) & 0x1;
      int16_t b = (buf[i/4] >> ((i%4)*2+1)) & 0x1;
      r[i] = a - b;
    }
    return;
  }

  if(eta == 2) {
    for(i = 0; i < n/8; i++) {
      uint32_t t = (uint32_t)buf[4*i + 0]
                 | ((uint32_t)buf[4*i + 1] << 8)
                 | ((uint32_t)buf[4*i + 2] << 16)
                 | ((uint32_t)buf[4*i + 3] << 24);
      uint32_t d = t & 0x55555555;
      d += (t >> 1) & 0x55555555;
      for(j = 0; j < 8; j++) {
        int16_t a = (d >> (4*j+0)) & 0x3;
        int16_t b = (d >> (4*j+2)) & 0x3;
        r[8*i+j] = a - b;
      }
    }
    return;
  }

  if(eta == 3) {
    for(i = 0; i < n/4; i++) {
      uint32_t t = (uint32_t)buf[3*i + 0]
                 | ((uint32_t)buf[3*i + 1] << 8)
                 | ((uint32_t)buf[3*i + 2] << 16);
      uint32_t d = t & 0x00249249;
      d += (t >> 1) & 0x00249249;
      d += (t >> 2) & 0x00249249;
      for(j = 0; j < 4; j++) {
        int16_t a = (d >> (6*j+0)) & 0x7;
        int16_t b = (d >> (6*j+3)) & 0x7;
        r[4*i+j] = a - b;
      }
    }
    return;
  }

  if(eta == 7 || eta == 9) {
    for(i = 0; i < n/4; i++) {
      int bits_per_coeff = 2 * eta;
      int bytes_per_group = eta;
      uint64_t lo = 0;
      uint32_t hi = 0;
      for(j = 0; j < bytes_per_group && j < 8; j++)
        lo |= (uint64_t)buf[bytes_per_group*i + j] << (8*j);
      if(bytes_per_group > 8)
        hi = buf[bytes_per_group*i + 8];

      for(j = 0; j < 4; j++) {
        unsigned int shift = bits_per_coeff * j;
        uint32_t v;
        if(shift <= 64 - bits_per_coeff) {
          v = (uint32_t)(lo >> shift);
        } else {
          v = (uint32_t)((lo >> shift) | ((uint64_t)hi << (64 - shift)));
        }
        v &= (1u << bits_per_coeff) - 1u;
        r[4*i+j] = (int16_t)popcount_u32(v & ((1u << eta) - 1u))
                 - (int16_t)popcount_u32((v >> eta) & ((1u << eta) - 1u));
      }
    }
    return;
  }

  for(i = 0; i < n; i++)
    r[i] = 0;
}

static int compare_poly_with_ref(const poly *got, const int16_t *ref, int eta)
{
  unsigned int i;
  for(i = 0; i < WEAVER_N; i++) {
    if(got->coeffs[i] != ref[i]) {
      printf("Mismatch at coeff %u: got=%d ref=%d\n", i, got->coeffs[i], ref[i]);
      return 0;
    }
    if(got->coeffs[i] < -eta || got->coeffs[i] > eta) {
      printf("Out of range at coeff %u: got=%d eta=%d\n", i, got->coeffs[i], eta);
      return 0;
    }
  }
  return 1;
}

int main(void)
{
  poly r;
  int16_t ref[WEAVER_N];
  uint8_t buf1[WEAVER_ETA1 * WEAVER_N / 4];
  uint8_t buf2[WEAVER_ETA2 * WEAVER_N / 4];
  unsigned int i;

  for(i = 0; i < sizeof(buf1); i++)
    buf1[i] = (uint8_t)(i * 17u + 3u);
  for(i = 0; i < sizeof(buf2); i++)
    buf2[i] = (uint8_t)(i * 29u + 11u);

  cbd_ref(ref, buf1, WEAVER_N, WEAVER_ETA1);
  cbd_eta1(&r, buf1);
  if(!compare_poly_with_ref(&r, ref, WEAVER_ETA1)) {
    printf("FAIL cbd_eta1 (mode=%d, eta1=%d)\n", WEAVER_MODE, WEAVER_ETA1);
    return 1;
  }

  cbd_ref(ref, buf2, WEAVER_N, WEAVER_ETA2);
  cbd_eta2(&r, buf2);
  if(!compare_poly_with_ref(&r, ref, WEAVER_ETA2)) {
    printf("FAIL cbd_eta2 (mode=%d, eta2=%d)\n", WEAVER_MODE, WEAVER_ETA2);
    return 1;
  }

  printf("PASS cbd test: mode=%d N=%d eta1=%d eta2=%d\n",
         WEAVER_MODE, WEAVER_N, WEAVER_ETA1, WEAVER_ETA2);
  return 0;
}
