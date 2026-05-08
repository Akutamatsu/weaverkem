/**
 * poly_invq.c — WEAVER-Inv: Randomized Lifting via Inv_q (8-bit Edition)
 *
 * 修复点：
 * 1) y=0 桶使用模 q 连续区间起点（环绕起点）
 * 2) 使用 Lemire rejection sampling 做严格均匀采样
 * 3) 保持对外函数签名与 invq.h 一致，可直接编译链接
 */

#include <stdint.h>
#include <string.h>
#ifdef INVQ_STATS
#include <stdio.h>
#endif
#include "params.h"
#include "poly.h"
#include "invq.h"
#include "symmetric.h"

#include "invq_table_data.h"

#ifdef INVQ_STATS
static uint64_t g_invq_reject_count = 0;
static uint64_t g_invq_fallback_count = 0;
#endif



// static uint32_t compress_q(uint32_t x, int d)
// {
//   uint32_t num_buckets = 1u << d;
//   return (uint32_t)(((uint64_t)x * num_buckets + KYBER_Q / 2) / KYBER_Q)
//          & (num_buckets - 1);
// }

// static void invq_init(invq_table_t *tbl, int d)
// {
//   uint32_t x, y;
//   uint32_t num_buckets = 1u << d;
//   uint16_t count[KYBER_Q];
//   uint32_t first[KYBER_Q];
//   uint32_t lo0 = 0;

//   tbl->d = d;

//   for(y = 0; y < num_buckets; y++) {
//     count[y] = 0;
//     first[y] = KYBER_Q;
//   }

//   for(x = 0; x < (uint32_t)KYBER_Q; x++) {
//     y = compress_q(x, d);
//     if(first[y] == (uint32_t)KYBER_Q)
//       first[y] = x;
//     count[y]++;
//   }

//   /* y=0: 找到 q-1 向下连续映射到 0 的尾段起点 */
//   for(x = (uint32_t)KYBER_Q; x > 0; x--) {
//     if(compress_q(x - 1, d) == 0)
//       lo0 = x - 1;
//     else
//       break;
//   }
  
//   first[0] = lo0;
//   tbl->small_size = 0;
//   tbl->large_size = 0;
//   tbl->large_bucket_count = 0;

//   for(y = 0; y < num_buckets; y++) {
//     tbl->bucket_lo[y] = first[y];
//     tbl->bucket_size[y] = (uint8_t)count[y];

//     if(tbl->large_size < count[y])
//       tbl->large_size = count[y];
//     if(tbl->small_size == 0 || tbl->small_size > count[y])
//       tbl->small_size = count[y];
//   }
// }

// void invq_global_init(void)
// {
// #if (KYBER_PK_POLYVECBYTES == (KYBER_K * KYBER_N * 8 / 8))
//   invq_init(&invq_pk_table, 8);
// #elif (KYBER_PK_POLYVECBYTES == (KYBER_K * KYBER_N * 9 / 8))
//   invq_init(&invq_pk_table, 9);
// #elif (KYBER_PK_POLYVECBYTES == (KYBER_K * KYBER_N * 10 / 8))
//   invq_init(&invq_pk_table, 10);
// #else
// #error "Unsupported PK_COMPRESS precision for Inv_q"
// #endif
// }

void invq_global_init(void)
{

}


/* 精确均匀映射到 [0, size-1] 的 8 位极限版本 */
static inline int sample_offset_exact_8(uint8_t rand8, uint32_t size, uint32_t *offset)
{
  // 8位随机数 * size，结果最大为 255 * 7 = 1785
  uint32_t m = (uint32_t)rand8 * size;
  
  // 低 8 位是分数残余
  uint8_t l = (uint8_t)m;
  
  // 阈值计算: (2^8) % size，即 256 % size
  uint8_t t = (uint8_t)(256UL % size); 

  if(l < t) {
#ifdef INVQ_STATS
    g_invq_reject_count++;
#endif
    return 0; // 拒绝
  }

  // 高 8 位是有效偏移
  *offset = m >> 8;
  return 1; // 成功
}

/* 内部：带游标的真拒绝采样版本 */
static int poly_invq_with_cursor(poly *r,
                                 const uint8_t *randbuf,
                                 size_t max_len,
                                 size_t *pos,
                                 const invq_table_t *tbl)
{
  unsigned int i;

  for(i = 0; i < KYBER_N; i++) {
    uint16_t y = (uint16_t)r->coeffs[i];
    uint32_t lo = tbl->bucket_lo[y];
    uint32_t size = tbl->bucket_size[y];
    uint32_t offset;

    for(;;) {
      uint8_t rand8;

      // 现在每次只消耗 1 字节
      if((*pos + 1) > max_len)
        return -1;

      // 直接取用，连位运算拼装都省了
      rand8 = randbuf[*pos];
      *pos += 1;

      // 调用 8 位采样函数
      if(sample_offset_exact_8(rand8, size, &offset))
        break;
    }

    {
      uint32_t res = lo + offset;
      res -= KYBER_Q & (uint32_t)(-(int32_t)(res >= KYBER_Q));
      r->coeffs[i] = (int16_t)res;
    }
  }

  return 0;
}

/* 对外接口保持不变：固定输入长度，不做重取扩展 */
void poly_invq(poly *r, const uint8_t *randbuf, const invq_table_t *tbl)
{
  unsigned int i;
  
  for(i = 0; i < KYBER_N; i++) {
    uint16_t y = (uint16_t)r->coeffs[i];
    uint32_t lo = tbl->bucket_lo[y];
    uint32_t size = tbl->bucket_size[y];
    
    // 读取 1 字节
    uint8_t rand8 = randbuf[i];

    /* 单样本近似路径：保持接口兼容与固定耗时 */
    uint32_t offset = (uint32_t)(((uint32_t)rand8 * size) >> 8);
    uint32_t res = lo + offset;
    res -= KYBER_Q & (uint32_t)(-(int32_t)(res >= KYBER_Q));
    r->coeffs[i] = (int16_t)res;
  }
}
#include "speed/cpucycles.h"
uint64_t g_cycles_invq_prf = 0;
uint64_t g_cycles_invq_sample = 0;
void polyvec_invq(polyvec *v,
                  const uint8_t seed[KYBER_SYMBYTES],
                  uint8_t nonce,
                  const invq_table_t *tbl)
{
  unsigned int i;
  size_t pos = 0;
  size_t randbuflen = (size_t)KYBER_K * KYBER_N + 64;
  uint8_t _randbuf[(size_t)KYBER_K * KYBER_N + 64];

  // ==========================================
  // 2. 内部打点
  // ==========================================
  uint64_t t0, t1, t2;
  t0 = cpucycles();

  prf(_randbuf, randbuflen, seed, nonce);

  t1 = cpucycles();

  for(i = 0; i < KYBER_K; i++) {
    if(poly_invq_with_cursor(&v->vec[i], _randbuf, randbuflen, &pos, tbl) != 0) {
#ifdef INVQ_STATS
      g_invq_fallback_count++;
#endif
      /* 极小概率耗尽时回退到兼容路径，保证可用 */
      size_t base = i * (size_t)KYBER_N;
      if(base + (size_t)KYBER_N > randbuflen)
        base = 0;

      for(; i < KYBER_K; i++) {
        poly_invq(&v->vec[i], _randbuf + base, tbl);
        base += (size_t)KYBER_N;
        if(base + (size_t)KYBER_N > randbuflen)
          base = 0;
      }
      goto finish_timing; 
    }
  }

finish_timing:
  t2 = cpucycles();
  
  // ==========================================
  // 3. 累加到全局变量
  // ==========================================
  g_cycles_invq_prf += (t1 - t0);
  g_cycles_invq_sample += (t2 - t1);
}

#ifdef INVQ_STATS
__attribute__((destructor))
static void invq_stats_dump(void)
{
  fprintf(stderr, "[INVQ_STATS] reject=%llu fallback=%llu\n",
          (unsigned long long)g_invq_reject_count,
          (unsigned long long)g_invq_fallback_count);
}
#endif