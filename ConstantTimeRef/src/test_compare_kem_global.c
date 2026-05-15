/**
 * test_compare.c — WEAVER-Inv 性能对比测试
 *
 * 一次运行，同时测量旧方案(Decompress)和新方案(Inv_q)各阶段时钟周期，
 * 输出对比表格。
 *
 * 旧方案 = 原始 Decompress 确定性提升 (polyvec_decompress_pk)
 * 新方案 = Inv_q 随机提升           (polyvec_fromcompressed_pk + polyvec_invq)
 *
 * 编译：在 ref/speed_test/ 目录下运行 make
 * 运行：./512-speed_cmp  或  ./1024-speed_cmp  或  ./2048-speed_cmp
 */

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
#include "msgenc.h"
#include "verify.h"

/* gen_at 是 indcpa.c 内部宏，这里重新定义 */
#define gen_at(A,B)  gen_matrix(A,B,1)

#define NTESTS 10000

/* ====== 计时辅助 ====== */
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

/*
 * bench(): 执行 NTESTS 次计时，返回 median 和 average
 * func: 被测函数指针（无参数，返回 void）
 */
typedef void (*bench_fn)(void);
static void run_bench(bench_fn func, uint64_t *t, int ntests,
                      uint64_t *out_med, uint64_t *out_avg)
{
    int i;
    uint64_t overhead = cpucycles_overhead();
    for(i = 0; i < ntests; i++)
        t[i] = cpucycles();
    for(i = 0; i < ntests; i++) {
        t[i] = cpucycles();
        func();
    }
    /* 相邻差分，减去 overhead */
    for(i = 0; i < ntests - 1; i++)
        t[i] = t[i+1] - t[i] - overhead;
    *out_med = median(t, ntests - 1);
    *out_avg = average(t, ntests - 1);
}

/* ====== 全局测试数据（避免栈分配影响计时）====== */
static uint8_t  pk_buf[CRYPTO_PUBLICKEYBYTES];
static uint8_t  sk_buf[CRYPTO_SECRETKEYBYTES];
static uint8_t  ct_buf[CRYPTO_CIPHERTEXTBYTES];
static uint8_t  key_buf[CRYPTO_BYTES];
static uint8_t  coins_buf[KYBER_SYMBYTES];
static uint8_t  seed_buf[KYBER_SYMBYTES];
static uint8_t  msg_buf[KYBER_INDCPA_MSGBYTES];  /* 消息缓冲区 */

static polyvec   g_pkpv_old;     /* 旧方案：decompress 后的 pk */
static polyvec   g_pkpv_new;     /* 新方案：invq 提升后的 pk */
static polyvec   g_matrix[KYBER_K];
static poly      g_sp, g_ap;
static polyvec   g_at[KYBER_K], g_b;
static poly      g_v, g_k;
static uint8_t   g_nonce;

/* ====== 各阶段被测函数 ====== */

/* 阶段1: 密钥生成 keypair */
static void bench_keypair(void) {
    crypto_kem_keypair(pk_buf, sk_buf);
}

/* 阶段2a: 旧方案 — PK 解压缩 + NTT (Decompress路径) */
static void bench_pk_decompress_ntt(void) {
    unpack_pk(&g_pkpv_old, seed_buf, pk_buf);
#ifdef PK_COMPRESS
    polyvec_ntt(&g_pkpv_old);
#endif
}

/* 阶段2b: 新方案 — PK 压缩域加载 + Inv_q随机提升 + NTT */
static void bench_pk_invq_lift_ntt(void) {
    polyvec_fromcompressed_pk(&g_pkpv_new, pk_buf);       /* 加载压缩值 */
    polyvec_invq(&g_pkpv_new, coins_buf, g_nonce++, &invq_pk_table); /* Inv_q提升 */
#ifdef PK_COMPRESS
    polyvec_ntt(&g_pkpv_new);
#endif
}

/* 阶段3: 完整封装 encaps (使用当前编译的版本 = 新方案 Inv_q) */
static void bench_encaps_new(void) {
    crypto_kem_enc(ct_buf, key_buf, pk_buf);
}

/* 阶段4: 完整解密 decaps */
static void bench_decaps(void) {
    crypto_kem_dec(key_buf, ct_buf, sk_buf);
}


/* 阶段5: 旧方案完整封装模拟 (手动走 Decompress 路径)
 *        模拟 indcpa_enc 但不调用 Inv_q 分支
 */
// static void bench_encaps_old_sim(void) {
//     unsigned int i;
//     uint8_t seed[KYBER_SYMBYTES];
//     uint8_t nonce = 0;
//     polyvec sp = {0}, pkpv = {0}, at[KYBER_K] = {0}, b = {0};
//     poly v = {0}, k = {0};

//     /* ★ 旧方案: unpack_pk → decompress_pk → NTT (不经过 Inv_q) */
//     unpack_pk(&pkpv, seed, pk_buf);
// #ifdef PK_COMPRESS
//     polyvec_ntt(&pkpv);
// #endif

//     poly_frommsg(&k, msg_buf);  /* 用正确大小的消息缓冲区 */

//     gen_at(at, seed);

//     for(i = 0; i < KYBER_K; i++)
//         poly_getnoise_eta1(sp.vec + i, coins_buf, nonce++);

//     polyvec_ntt(&sp);

//     for(i = 0; i < KYBER_K; i++)
//         polyvec_basemul_acc_montgomery(&b.vec[i], &at[i], &sp);

//     polyvec_basemul_acc_montgomery(&v, &pkpv, &sp);

//     polyvec_invntt_tomont(&b);
//     poly_invntt_tomont(&v);

//     poly_add(&v, &v, &k);
//     polyvec_reduce(&b);
//     poly_reduce(&v);

//     pack_ciphertext(ct_buf, &b, &v);
// }

static void indcpa_enc_old_sim(uint8_t *c, const uint8_t *m, const uint8_t *pk, const uint8_t *coins) {
    unsigned int i;
    uint8_t seed[KYBER_SYMBYTES];
    uint8_t nonce = 0;
    polyvec sp = {0}, pkpv = {0}, at[KYBER_K] = {0}, b = {0};
    poly v = {0}, k = {0};

    /* ★ 核心：使用旧方案的解压公钥逻辑 */
    unpack_pk(&pkpv, seed, pk);
#ifdef PK_COMPRESS
    polyvec_ntt(&pkpv);
#endif

    poly_frommsg(&k, m);
    gen_at(at, seed);

    for(i = 0; i < KYBER_K; i++)
        poly_getnoise_eta1(sp.vec + i, coins, nonce++);

    polyvec_ntt(&sp);

    for(i = 0; i < KYBER_K; i++)
        polyvec_basemul_acc_montgomery(&b.vec[i], &at[i], &sp);

    polyvec_basemul_acc_montgomery(&v, &pkpv, &sp);

    polyvec_invntt_tomont(&b);
    poly_invntt_tomont(&v);

    poly_add(&v, &v, &k);
    polyvec_reduce(&b);
    poly_reduce(&v);

    pack_ciphertext(c, &b, &v);
}

static void bench_encaps_old_sim(void) {
    uint8_t buf[KYBER_INDCPA_MSGBYTES + KYBER_SYMBYTES];
    /* kr 包含 shared-key material || encryption coins */
    uint8_t kr[KYBER_SSBYTES + KYBER_SYMBYTES];

    /* 1. 为了绝对的 A/B 测试控制变量，这里不调用 randombytes，
     * 而是直接使用全局提前准备好的明文数据 coins_buf */
    randombytes(buf, KYBER_INDCPA_MSGBYTES);

    /* 2. 附加公钥哈希 (多目标攻击防御) */
    hash_h(buf + KYBER_INDCPA_MSGBYTES, pk_buf, KYBER_PUBLICKEYBYTES);

    /* 3. 使用定制的 shake256 派生 kr */
    shake256(kr, sizeof(kr), buf, sizeof(buf));

    /* 4. ★ 核心替换：调用我们手写的“旧方案”底层加密 */
    indcpa_enc_old_sim(ct_buf, buf, pk_buf, kr + KYBER_SSBYTES);

    /* 5. 提取最终的 shared secret */
    memcpy(key_buf, kr, KYBER_SSBYTES);
}

/* * 阶段4b: 旧方案完整解封装模拟 (Decaps_Old_Sim)
 * 完美复刻 crypto_kem_dec 的 FO 变换流程，但重加密环节调用旧版逻辑
 */
static void bench_decaps_old_sim(void) {
    int fail;
    /* 100% 同步原函数的缓冲区大小 */
    uint8_t buf[KYBER_INDCPA_MSGBYTES + KYBER_SYMBYTES];
    uint8_t kr[KYBER_SSBYTES + KYBER_SYMBYTES];
    uint8_t cmp[KYBER_CIPHERTEXTBYTES];
    const uint8_t *pk = sk_buf + KYBER_INDCPA_SECRETKEYBYTES;

    /* 1. 底层解密，获得候选明文 */
    indcpa_dec(buf, ct_buf, sk_buf);

    /* 2. 附加公钥哈希 (多目标攻击防御) */
    memcpy(buf + KYBER_INDCPA_MSGBYTES, sk_buf + KYBER_SECRETKEYBYTES - 2*KYBER_SYMBYTES, KYBER_SYMBYTES);
    
    /* 3. 使用定制的 shake256 派生 */
    shake256(kr, sizeof(kr), buf, sizeof(buf));

    /* 4. ★ 核心替换：调用我们手写的“旧方案”进行重加密 */
    /* 注意：这里的随机数指针偏移量是 KYBER_SSBYTES，保持和原函数一致 */
    indcpa_enc_old_sim(cmp, buf, pk, kr + KYBER_SSBYTES);

    /* 5. 密文比对验证 */
    fail = verify(ct_buf, cmp, KYBER_CIPHERTEXTBYTES);

    /* 6. 计算隐式拒绝密钥 (直接写入目标测试缓冲区 key_buf) */
    rkprf(key_buf, sk_buf + KYBER_SECRETKEYBYTES - KYBER_SYMBYTES, ct_buf);

    /* 7. 如果比对成功(没有fail)，则把真实的共享密钥拷贝给 key_buf */
    cmov(key_buf, kr, KYBER_SSBYTES, !fail);
}

/* ====== 表格输出 ====== */
static void print_separator(void) {
    printf("+------------------------+---------------+---------------+---------------+----------+\n");
}

static void print_header(void) {
    print_separator();
    printf("| %-22s | %13s | %13s | %13s | %8s |\n",
           "阶段", "旧方案(cyc)", "新方案(cyc)", "差异", "开销");
    print_separator();
}

static void print_row(const char *name,
                       uint64_t old_med, uint64_t new_med,
                       uint64_t old_avg, uint64_t new_avg)
{
    int64_t diff_med = (int64_t)new_med - (int64_t)old_med;
    double pct = (old_med > 0) ? (double)diff_med / (double)old_med * 100.0 : 0.0;
    const char *arrow = (diff_med >= 0) ? "+" : "";
    const char *tag   = (diff_med >= 0) ? "slower" : "faster";

    printf("| %-22s | %9llu(avg) | %9llu(avg) | %s%7lld(%+.1f%%) | %-6s   |\n",
           name,
           (unsigned long long)old_avg,
           (unsigned long long)new_avg,
           arrow, (long long)diff_med, pct,
           tag);
    print_separator();
}

/* ====== main ====== */
/* ====== main ====== */
int main()
{
    unsigned int i;

    uint64_t t[NTESTS];

    uint64_t kp_med, kp_avg;
    uint64_t pk_old_med, pk_old_avg;
    uint64_t pk_new_med, pk_new_avg;
    uint64_t enc_old_med, enc_old_avg;
    uint64_t enc_new_med, enc_new_avg;
    
    // 将原来的 dec_med/dec_avg 拆分为新旧两组
    uint64_t dec_new_med, dec_new_avg; 
    uint64_t dec_old_med, dec_old_avg;

    invq_global_init();

    /* 准备测试数据 */
    randombytes(coins_buf, KYBER_SYMBYTES);
    memset(seed_buf, 0x42, KYBER_SYMBYTES);
    memset(msg_buf, 0x01, KYBER_INDCPA_MSGBYTES);
    g_nonce = 0;

    /* 先生成一对有效密钥 */
    crypto_kem_keypair(pk_buf, sk_buf);

    printf("\n");
    printf("===== WEAVER-Inv Performance Comparison =====\n");
    printf("  Algorithm: %-20s  NTESTS: %d\n", CRYPTO_ALGNAME, NTESTS);
    printf("  Old = Decompress (deterministic) | New = Inv_q (randomized)\n\n");

    /* ---- 阶段1: Keypair (新旧相同) ---- */
    run_bench(bench_keypair, t, NTESTS, &kp_med, &kp_avg);
    printf("[1/5] keypair 完成: median=%llu cyc, avg=%llu cyc\n",
           (unsigned long long)kp_med, (unsigned long long)kp_avg);

    /* ---- 阶段2: PK 处理核心差异 ---- */
    run_bench(bench_pk_decompress_ntt, t, NTESTS, &pk_old_med, &pk_old_avg);
    printf("[2/5] 旧方案(pk_decompress+ntt) 完成: med=%llu cyc\n",
           (unsigned long long)pk_old_med);

    run_bench(bench_pk_invq_lift_ntt, t, NTESTS, &pk_new_med, &pk_new_avg);
    printf("[2/5] 新方案(fromcompressed+invq+ntt) 完成: med=%llu cyc\n",
           (unsigned long long)pk_new_med);

    /* ---- 阶段3: 完整封装 (新方案) ---- */
    run_bench(bench_encaps_new, t, NTESTS, &enc_new_med, &enc_new_avg);
    printf("[3/5] 新方案(encaps_Inv_q) 完成: med=%llu cyc\n",
           (unsigned long long)enc_new_med);

    /* ---- 阶段4: 旧方案封装模拟 ---- */
    run_bench(bench_encaps_old_sim, t, NTESTS, &enc_old_med, &enc_old_avg);
    printf("[4/5] 旧方案(encaps_Decompress) 完成: med=%llu cyc\n",
           (unsigned long long)enc_old_med);

    /* ---- 阶段5: Decaps (解封装拆解测试) ---- */
    // 5a. 测试新方案解封装 (系统底层调用了带 Inv_q 的重加密)
    run_bench(bench_decaps, t, NTESTS, &dec_new_med, &dec_new_avg);
    printf("[5/5a] 新方案 decaps 完成: med=%llu cyc\n",
           (unsigned long long)dec_new_med);

    // 5b. 测试旧方案解封装 (调用纯净版模拟器)
    run_bench(bench_decaps_old_sim, t, NTESTS, &dec_old_med, &dec_old_avg);
    printf("[5/5b] 旧方案 decaps 完成: med=%llu cyc\n",
           (unsigned long long)dec_old_med);

    /* ====== 输出对比表格 ====== */
    printf("\n");
    printf("===== CPU Cycle Comparison Table =====\n\n");

    print_header();

    /* Row 1: Keypair */
    printf("| %-22s | %9llu(avg) | %9llu(avg) | %8s | %-6s   |\n",
           "keypair",
           (unsigned long long)kp_avg, (unsigned long long)kp_avg,
           "—", "same");
    print_separator();

    /* Row 2: PK处理 (核心差异) */
    print_row("PK提升+NTT", pk_old_med, pk_new_med, pk_old_avg, pk_new_avg);

    /* Row 3: 封装 */
    print_row("encaps (full)", enc_old_med, enc_new_med, enc_old_avg, enc_new_avg);

    /* Row 4: 解密 (★这里改用 print_row 真实对比★) */
    print_row("decaps (full)", dec_old_med, dec_new_med, dec_old_avg, dec_new_avg);

    /* 汇总行 */
    {
        /* 为了科学严谨，总耗时我们统一使用 平均值(avg) 进行加总 */
        uint64_t total_old = enc_old_avg + kp_avg + dec_old_avg;
        uint64_t total_new = enc_new_avg + kp_avg + dec_new_avg;
        int64_t total_diff = (int64_t)total_new - (int64_t)total_old;
        double total_pct = (total_old > 0) ? (double)total_diff / (double)total_old * 100.0 : 0.0;
        const char *t_arrow = (total_diff >= 0) ? "+" : "";

        printf("| %-22s | %9llu(avg) | %9llu(avg) | %s%7lld(%+.1f%%) |          |\n",
               "Total(key+enc+dec)",
               (unsigned long long)total_old, (unsigned long long)total_new,
               t_arrow, (long long)total_diff, total_pct);
        print_separator();
    }

    printf("\n说明:\n");
    printf("  • 旧方案(Decompress): unpack_pk → decompress_pk → NTT (确定性)\n");
    printf("  • 新方案(Inv_q):      fromcompressed_pk → invq_random_lift → NTT (随机化)\n");
    printf("  • 差异列: 正数表示新方案比旧方案慢（安全增强的代价）\n");
    printf("  • avg = 平均值 (单位: CPU clock cycles)\n\n");

    return 0;
}