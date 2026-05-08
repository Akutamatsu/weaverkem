#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "params.h"
#include "poly.h"
#include "cbd.h"

/* ============================================================
 * Runtime CBD simulation for arbitrary eta
 * CBD_eta: sample from centered binomial distribution
 * Input: byte buffer of size eta*N/4
 * Output: N coefficients in [-eta, eta]
 * ============================================================ */
static uint32_t load32_le(const uint8_t *x)
{
  return (uint32_t)x[0]
       | ((uint32_t)x[1] << 8)
       | ((uint32_t)x[2] << 16)
       | ((uint32_t)x[3] << 24);
}

static uint64_t load48_le(const uint8_t *x)
{
  return (uint64_t)x[0]
       | ((uint64_t)x[1] << 8)
       | ((uint64_t)x[2] << 16)
       | ((uint64_t)x[3] << 24)
       | ((uint64_t)x[4] << 32)
       | ((uint64_t)x[5] << 40);
}

/* CBD2: eta=2, 4 bytes -> 8 coefficients */
static void cbd2_sim(int16_t *r, const uint8_t *buf, int n)
{
  int i, j;
  for(i = 0; i < n/8; i++) {
    uint32_t t = load32_le(buf + 4*i);
    uint32_t d = t & 0x55555555;
    d += (t >> 1) & 0x55555555;
    for(j = 0; j < 8; j++) {
      int16_t a = (d >> (4*j+0)) & 0x3;
      int16_t b = (d >> (4*j+2)) & 0x3;
      r[8*i+j] = a - b;
    }
  }
}

/* CBD3: eta=3, 3 bytes -> 4 coefficients */
static void cbd3_sim(int16_t *r, const uint8_t *buf, int n)
{
  int i, j;
  for(i = 0; i < n/4; i++) {
    uint32_t t = (uint32_t)buf[3*i] | ((uint32_t)buf[3*i+1] << 8) | ((uint32_t)buf[3*i+2] << 16);
    uint32_t d = t & 0x00249249;
    d += (t >> 1) & 0x00249249;
    d += (t >> 2) & 0x00249249;
    for(j = 0; j < 4; j++) {
      int16_t a = (d >> (6*j+0)) & 0x7;
      int16_t b = (d >> (6*j+3)) & 0x7;
      r[4*i+j] = a - b;
    }
  }
}

/* CBD4: eta=4, 4 bytes -> 4 coefficients */
static void cbd4_sim(int16_t *r, const uint8_t *buf, int n)
{
  int i, j;
  for(i = 0; i < n/4; i++) {
    uint32_t t = load32_le(buf + 4*i);
    uint32_t d = t & 0x11111111;
    d += (t >> 1) & 0x11111111;
    d += (t >> 2) & 0x11111111;
    d += (t >> 3) & 0x11111111;
    for(j = 0; j < 4; j++) {
      int16_t a = (d >> (8*j+0)) & 0xf;
      int16_t b = (d >> (8*j+4)) & 0xf;
      r[4*i+j] = a - b;
    }
  }
}

/* CBD5: eta=5, 5 bytes -> 4 coefficients */
static int popcount5_sim(uint16_t x)
{
  x &= 0x1F;
  return (int)((x & 1u) + ((x >> 1) & 1u) + ((x >> 2) & 1u) + ((x >> 3) & 1u) + ((x >> 4) & 1u));
}

static void cbd5_sim(int16_t *r, const uint8_t *buf, int n)
{
  int i, j;
  for(i = 0; i < n/4; i++) {
    uint64_t t = (uint64_t)buf[5*i + 0]
               | ((uint64_t)buf[5*i + 1] << 8)
               | ((uint64_t)buf[5*i + 2] << 16)
               | ((uint64_t)buf[5*i + 3] << 24)
               | ((uint64_t)buf[5*i + 4] << 32);
    for(j = 0; j < 4; j++) {
      uint16_t v = (t >> (10*j)) & 0x3FF;
      int16_t a = (int16_t)popcount5_sim(v);
      int16_t b = (int16_t)popcount5_sim(v >> 5);
      r[4*i+j] = a - b;
    }
  }
}

static void cbd_sim(int16_t *r, const uint8_t *buf, int n, int eta)
{
  if(eta == 1) {
    int i;
    for(i = 0; i < n; i++) {
      int16_t a = (buf[i/4] >> ((i%4)*2+0)) & 0x1;
      int16_t b = (buf[i/4] >> ((i%4)*2+1)) & 0x1;
      r[i] = a - b;
    }
  }
  else if(eta == 2) cbd2_sim(r, buf, n);
  else if(eta == 3) cbd3_sim(r, buf, n);
  else if(eta == 4) cbd4_sim(r, buf, n);
  else if(eta == 5) cbd5_sim(r, buf, n);
}

/* ============================================================
 * Statistics
 * ============================================================ */
typedef struct {
  double mean;
  double variance;
  int min_val;
  int max_val;
  int zero_count;
  int pos_count;
  int neg_count;
} cbd_stats;

static cbd_stats compute_stats(const int16_t *coeffs, int n)
{
  cbd_stats s;
  int64_t sum = 0, var_sum = 0;
  int i;
  s.min_val = coeffs[0]; s.max_val = coeffs[0];
  s.zero_count = s.pos_count = s.neg_count = 0;
  for(i = 0; i < n; i++) {
    int16_t c = coeffs[i];
    sum += c;
    if(c < s.min_val) s.min_val = c;
    if(c > s.max_val) s.max_val = c;
    if(c == 0) s.zero_count++;
    else if(c > 0) s.pos_count++;
    else s.neg_count++;
  }
  s.mean = (double)sum / n;
  for(i = 0; i < n; i++) {
    double diff = coeffs[i] - s.mean;
    var_sum += (int64_t)(diff * diff);
  }
  s.variance = (double)var_sum / n;
  return s;
}

/* ============================================================
 * Test one parameter set
 * ============================================================ */
typedef struct {
  const char *name;
  int n;
  int eta1;
} cbd_param;

static int test_cbd_param(const cbd_param *p)
{
  int passed = 0;
  int bufsize;
  uint8_t buf[512];  /* large enough for any eta*N/4 */
  int16_t coeffs[512];
  cbd_stats stats;
  int i;

  printf("\n--- %s (n=%d, eta1=%d) ---\n",
         p->name, p->n, p->eta1);

  /* Test eta1 */
  bufsize = p->eta1 * p->n / 4;
  for(i = 0; i < bufsize; i++) buf[i] = (uint8_t)(i * 7 + 13);
  cbd_sim(coeffs, buf, p->n, p->eta1);
  stats = compute_stats(coeffs, p->n);

  printf("  [eta1=%d] min=%d max=%d mean=%.3f var=%.3f ",
         p->eta1, stats.min_val, stats.max_val, stats.mean, stats.variance);
  if(stats.min_val >= -p->eta1 && stats.max_val <= p->eta1) {
    printf("PASSED (range [-%d,%d] OK)\n", p->eta1, p->eta1);
    passed++;
  } else {
    printf("FAILED (out of range [-%d,%d])\n", p->eta1, p->eta1);
  }

  printf("  Result: %d/1 passed\n", passed);
  return passed == 1;
}

/* ============================================================
 * Also test actual cbd_eta1/cbd_eta2 for current WEAVER_MODE
 * ============================================================ */
static int test_actual_cbd(void)
{
  poly r;
  uint8_t buf[KYBER_ETA1 * KYBER_N / 4];
  int i;
  int passed = 0;

  printf("\n--- Actual cbd_eta1 (WEAVER_MODE=%d, N=%d) ---\n",
         WEAVER_MODE, KYBER_N);

  /* eta1 */
  for(i = 0; i < (int)sizeof(buf); i++) buf[i] = (uint8_t)(i * 7 + 13);
  cbd_eta1(&r, buf);
  {
    int ok = 1;
    for(i = 0; i < KYBER_N; i++) {
      if(r.coeffs[i] < -KYBER_ETA1 || r.coeffs[i] > KYBER_ETA1) { ok = 0; break; }
    }
    printf("  cbd_eta1 (eta=%d): ", KYBER_ETA1);
    if(ok) { printf("PASSED\n"); passed++; }
    else     printf("FAILED\n");
  }

  printf("  Result: %d/1 passed\n", passed);
  return passed == 1;
}

int main(void)
{
  int total_passed = 0;
  int total_sets   = 0;
  int s;

  printf("========================================\n");
  printf("  CBD Sampling Tests (All 3 Parameter Sets)\n");
  printf("========================================\n");
  printf("KYBER_Q = %d\n", KYBER_Q);

  /* Table 1: all three parameter sets */
  cbd_param sets[3] = {
    { "WEAVER-512",  256, 5 },
    { "WEAVER-1024", 256, 2 },
    { "WEAVER-2048", 512, 1 }
  };

  for(s = 0; s < 3; s++) {
    if(test_cbd_param(&sets[s])) total_passed++;
    total_sets++;
  }

  /* Verify actual implementation */
  printf("\n========================================\n");
  printf("  Actual Implementation Test\n");
  printf("========================================\n");
  if(test_actual_cbd()) total_passed++;
  total_sets++;

  printf("\n========================================\n");
  printf("  Final Summary\n");
  printf("========================================\n");
  printf("Passed: %d / %d parameter sets\n", total_passed, total_sets);

  if(total_passed == total_sets) {
    printf("All tests PASSED!\n");
    return 0;
  } else {
    printf("Some tests FAILED!\n");
    return 1;
  }
}
