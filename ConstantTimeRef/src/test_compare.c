#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "api.h"
#include "params.h"
#include "indcpa.h"
#include "poly.h"
#include "polyvec.h"
#include "symmetric.h"
#include "rng.h"
#include "cpucycles.h"
#include "invq.h"

#define NTESTS 10000

/* ====== 引入 poly_invq.c 中的全局累加探针 ====== */
extern uint64_t g_cycles_invq_prf;
extern uint64_t g_cycles_invq_sample;

/* ====== 计时辅助函数 ====== */
static int cmp_uint64(const void *a, const void *b) {
    if(*(uint64_t *)a < *(uint64_t *)b) return -1;
    if(*(uint64_t *)a > *(uint64_t *)b) return 1;
    return 0;
}

static uint64_t median(uint64_t *l, size_t llen) {
    qsort(l, llen, sizeof(uint64_t), cmp_uint64);
    if(llen % 2) return l[llen/2];
    else return (l[llen/2-1] + l[llen/2]) / 2;
}

static uint64_t average(uint64_t *t, size_t tlen) {
    size_t i;
    uint64_t acc = 0;
    for(i = 0; i < tlen; i++) acc += t[i];
    return acc / tlen;
}

typedef void (*bench_fn)(void);
static void run_bench(bench_fn func, uint64_t *t, int ntests,
                      uint64_t *out_med, uint64_t *out_avg)
{
    int i;
    uint64_t overhead = cpucycles_overhead();
    for(i = 0; i < ntests; i++) t[i] = cpucycles();
    for(i = 0; i < ntests; i++) {
        t[i] = cpucycles();
        func();
    }
    for(i = 0; i < ntests - 1; i++)
        t[i] = t[i+1] - t[i] - overhead;
    *out_med = median(t, ntests - 1);
    *out_avg = average(t, ntests - 1);
}

/* ====== 全局测试数据 ====== */
static uint8_t  pk_buf[CRYPTO_PUBLICKEYBYTES];
static uint8_t  sk_buf[CRYPTO_SECRETKEYBYTES];
static uint8_t  coins_buf[WEAVER_SYMBYTES];
static uint8_t  seed_buf[WEAVER_SYMBYTES];
static polyvec  g_pkpv_old, g_pkpv_new, g_pkpv_backup;
static uint8_t  g_nonce;

/* ====== 被测函数封装 ====== */

// 旧方案：解压 + NTT
static void bench_pk_old_full(void) {
    unpack_pk(&g_pkpv_old, seed_buf, pk_buf);
#ifdef PK_COMPRESS
    polyvec_ntt(&g_pkpv_old);
#endif
}

// 新方案：加载 + 随机提升 + NTT
static void bench_pk_new_full(void) {
    polyvec_fromcompressed_pk(&g_pkpv_new, pk_buf);
    polyvec_invq(&g_pkpv_new, coins_buf, g_nonce++, &invq_pk_table);
#ifdef PK_COMPRESS
    polyvec_ntt(&g_pkpv_new);
#endif
}

// 拆解步骤 1：加载
static void bench_step1_load(void) {
    polyvec_fromcompressed_pk(&g_pkpv_new, pk_buf);
}

// 拆解步骤 2：Inv_q 提升（这是探针工作的核心区域）
static void bench_step2_invq(void) {
    g_pkpv_new = g_pkpv_backup; // 恢复压缩态防止崩溃
    polyvec_invq(&g_pkpv_new, coins_buf, g_nonce++, &invq_pk_table);
}

// 拆解步骤 3：NTT
static void bench_step3_ntt(void) {
#ifdef PK_COMPRESS
    polyvec_ntt(&g_pkpv_new);
#endif
}

/* ====== 打印辅助 ====== */
static void print_line(void) {
    printf("+------------------------+---------------+---------------+---------------+----------+\n");
}

int main()
{
    uint64_t t[NTESTS];
    uint64_t old_med, old_avg, new_med, new_avg;
    uint64_t s1_med, s1_avg, s2_med, s2_avg, s3_med, s3_avg;

    invq_global_init();
    memset(seed_buf, 0x42, WEAVER_SYMBYTES);
    randombytes(coins_buf, WEAVER_SYMBYTES);
    g_nonce = 0;

    // 预准备数据
    uint8_t dummy_sk[CRYPTO_SECRETKEYBYTES];
    crypto_kem_keypair(pk_buf, dummy_sk);
    polyvec_fromcompressed_pk(&g_pkpv_backup, pk_buf);

    printf("\n===== WEAVER-Inv (8-bit) Performance Analysis =====\n");
    printf("  NTESTS: %d | Algorithm: %s\n\n", NTESTS, CRYPTO_ALGNAME);

    /* 1. 宏观方案对比 */
    run_bench(bench_pk_old_full, t, NTESTS, &old_med, &old_avg);
    run_bench(bench_pk_new_full, t, NTESTS, &new_med, &new_avg);

    /* 2. 步骤拆解测试 */
    run_bench(bench_step1_load, t, NTESTS, &s1_med, &s1_avg);
    
    // 清零探针累加器，确保只记录接下来这 10000 次 bench_step2_invq 的耗时
    g_cycles_invq_prf = 0;
    g_cycles_invq_sample = 0;
    run_bench(bench_step2_invq, t, NTESTS, &s2_med, &s2_avg);
    
    run_bench(bench_step3_ntt, t, NTESTS, &s3_med, &s3_avg);

    /* --- 输出表 1: 宏观对比 --- */
    printf("表 1: 新旧方案宏观对比 (PK提升 + NTT)\n");
    print_line();
    printf("| %-22s | %13s | %13s | %13s | %8s |\n", "阶段", "旧(Decompress)", "新(Inv_q)", "差异", "结论");
    print_line();
    
    int64_t diff = (int64_t)new_avg - (int64_t)old_avg;
    double pct = (old_avg > 0) ? (double)diff / (double)old_avg * 100.0 : 0.0;
    printf("| %-22s | %9llu(avg) | %9llu(avg) | +%7lld(%+.1f%%) | %-8s |\n",
           "PK处理总耗时", (unsigned long long)old_avg, (unsigned long long)new_avg, 
           (long long)diff, pct, (diff > 0 ? "slower" : "faster"));
    print_line();

    /* --- 输出表 2: 新方案内部拆解 --- */
    printf("\n表 2: 新方案 (Inv_q) 内部步骤拆解\n");
    printf("+--------------------------------+---------------+---------------+\n");
    printf("| 内部步骤                       | 中位数(cyc)   | 平均值(cyc)   |\n");
    printf("+--------------------------------+---------------+---------------+\n");
    printf("| 1. 加载压缩域 (Load)           | %13llu | %13llu |\n", 
           (unsigned long long)s1_med, (unsigned long long)s1_avg);
    printf("| 2. 随机提升 (Inv_q Sampling)   | %13llu | %13llu |\n", 
           (unsigned long long)s2_med, (unsigned long long)s2_avg);
    printf("| 3. NTT 变换 (NTT)              | %13llu | %13llu |\n", 
           (unsigned long long)s3_med, (unsigned long long)s3_avg);
    printf("+--------------------------------+---------------+---------------+\n");

    /* --- 输出表 3: Inv_q 探针细节 --- */
    printf("\n表 3: polyvec_invq 函数内部开销探针 (来自探针累加器)\n");
    printf("+--------------------------------+---------------+---------------+\n");
    printf("| 探针项                         |       -       | 平均值(cyc)   |\n");
    printf("+--------------------------------+---------------+---------------+\n");
    printf("| • PRF 随机流生成 (SHAKE256)    |       -       | %13llu |\n", 
           (unsigned long long)(g_cycles_invq_prf / NTESTS));
    printf("| • 8-bit 游标拒绝采样逻辑       |       -       | %13llu |\n", 
           (unsigned long long)(g_cycles_invq_sample / NTESTS));
    printf("+--------------------------------+---------------+---------------+\n");
    printf("|   探针实测总计                 |       -       | %13llu |\n", 
           (unsigned long long)((g_cycles_invq_prf + g_cycles_invq_sample) / NTESTS));
    printf("+--------------------------------+---------------+---------------+\n\n");

    return 0;
}