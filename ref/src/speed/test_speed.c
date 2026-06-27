#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include "api.h"
#include "params.h"
#include "indcpa.h"
#include "poly.h"
#include "polyvec.h"
#include "cpucycles.h"
#include "speed_print.h" // 完美保留你原版的打印模块

// 根据你最新的 Makefile，引入被拆分出来的新架构模块
#include "msgenc.h"      
#include "bch.h"     
#include "symmetric.h"
#include "kem.h" 

#ifndef WEAVER_USE_SHAKE
#include "drng.h"
DRNG_ctx drng_algorithm;
void randombytes_init(unsigned char *entropy_input,
                 unsigned char *personalization_string,
                 int security_strength)
{
    (void)personalization_string;

    init_random_number(&drng_algorithm, entropy_input, (security_strength/8));
}
#else
#include "rng.h" 
#endif

#define NTESTS 10000

uint64_t t[NTESTS];
uint8_t seed[WEAVER_SYMBYTES] = {0};

// === [新增] CSV 后台引擎，排序算法 ===
static int cmp_uint64(const void *a, const void *b) {
  if(*(uint64_t *)a < *(uint64_t *)b) return -1;
  if(*(uint64_t *)a > *(uint64_t *)b) return 1;
  return 0;
}

// === [修复] 静默追加写入 CSV 的辅助函数 ===
void append_to_csv(const char *op_name, uint64_t *cycles, int n) {
  // 动态生成包含安全等级的文件名
  char filename[128];
  snprintf(filename, sizeof(filename), "%s_performance.csv", CRYPTO_ALGNAME);

  FILE *fp = fopen(filename, "a");
  if(fp) {
    int valid_n = n - 1; 
    uint64_t sum = 0;
    
    for(int i = 0; i < valid_n; i++) {
        sum += cycles[i];
    }
    uint64_t avg = sum / valid_n;
    uint64_t median = cycles[valid_n / 2];
    
    fprintf(fp, "%s,%" PRIu64 ",%" PRIu64 "\n", op_name, median, avg);
    fclose(fp);
  }
}

// === [新增] 魔法宏：一行代码同时搞定屏幕输出和 Excel 写入 ===
#define PRINT_AND_CSV(name, t_array, n) \
  do { \
    print_results(name, t_array, n); \
    append_to_csv(name, t_array, n); \
  } while(0)


int main()
{
  unsigned int i;
  unsigned char pk[CRYPTO_PUBLICKEYBYTES] = {0};
  unsigned char sk[CRYPTO_SECRETKEYBYTES] = {0};
  unsigned char ct[CRYPTO_CIPHERTEXTBYTES] = {0};
  unsigned char key[CRYPTO_BYTES] = {0};
  uint8_t garbage_buf[WEAVER_GBYTES] = {0};
  polyvec matrix[WEAVER_K], s, e;
  poly ap;
  uint8_t msg[WEAVER_INDCPA_MSGBYTES] = {0};
  uint8_t ecc_buf[128] = {0}; // BCH 校验位缓冲
  uint8_t pcomp[WEAVER_POLYCOMPRESSEDBYTES] = {0};
  uint8_t pvcomp[WEAVER_POLYVECCOMPRESSEDBYTES] = {0};
  char filename[128];
  unsigned char entropy_input[64];

  snprintf(filename, sizeof(filename), "%s_performance.csv", CRYPTO_ALGNAME);

  printf("%s Performance Test Start..\n", CRYPTO_ALGNAME);
  
  // 初始化 CSV 文件，写入表头 
  FILE *csv_file = fopen(filename, "w");
  if(csv_file) {
      fprintf(csv_file, "Operation,Median (Cycles),Average (Cycles)\n");
      fclose(csv_file);
  }

  for (int i=0; i<sizeof(entropy_input); i++)
    entropy_input[i] = i;
  randombytes_init(entropy_input, NULL, 512);

  // ============== 0. 对称哈希与系统调用模块 ==============
  
  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    randombytes(key, WEAVER_SYMBYTES);
  }
  PRINT_AND_CSV("randombytes: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    // 模拟 KEM Encaps 中的 hash_h (对公钥进行 SHA3-256)
    hash_h(garbage_buf, pk, WEAVER_PUBLICKEYBYTES);
  }
  PRINT_AND_CSV("hash_h (PK): ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    // 模拟 KEM Encaps 中的 hash_g (SHA3-512)
    hash_g(garbage_buf, key, 2*WEAVER_SYMBYTES);
  }
  PRINT_AND_CSV("hash_g: ", t, NTESTS);
  
  

  // ============== 1. 核心数学与采样模块 ==============

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    gen_matrix(matrix, seed, 0);
  }
  PRINT_AND_CSV("gen_a: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    poly_getnoise_eta1(&ap, seed, 0); 
  }
  PRINT_AND_CSV("gen_s: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    poly_getnoise_eta2(&ap, seed, 0); 
  }
  PRINT_AND_CSV("gen_e: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    poly_ntt(&ap);
  }
  PRINT_AND_CSV("NTT: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    poly_invntt_tomont(&ap);
  }
  PRINT_AND_CSV("INVNTT: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    polyvec_basemul_acc_montgomery(&ap, &matrix[0], &s); 
  }
  PRINT_AND_CSV("vec_mult: ", t, NTESTS);

  // ============== 2. 压缩与解压缩模块 ==============

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    poly_compress(pcomp, &ap);
  }
  PRINT_AND_CSV("poly_compress: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    poly_decompress(&ap, pcomp);
  }
  PRINT_AND_CSV("poly_decompress: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    polyvec_compress(pvcomp, &matrix[0]);
  }
  PRINT_AND_CSV("polyvec_compress: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    polyvec_decompress(&matrix[0], pvcomp);
  }
  PRINT_AND_CSV("polyvec_decompress: ", t, NTESTS);

  // ============== 3. 底层 BCH 编解码与消息映射模块 ==============

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    encode_bch_high(msg, ELL_BAR_BYTES, ecc_buf);
  }
  PRINT_AND_CSV("BCH_encode: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    decode_bch_high(msg, ELL_DDOT_BYTES, ecc_buf);
  }
  PRINT_AND_CSV("BCH_decode: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    poly_frommsg(&ap, msg);
  }
  PRINT_AND_CSV("msg_encode: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    poly_tomsg(msg, &ap);
  }
  PRINT_AND_CSV("msg_decode: ", t, NTESTS);

  // ============== 4. 顶层 KEM 协议宏观模块 ==============
  
  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    // ct:密文输出, msg:输入的32字节/16字节明文, pk:公钥, seed:确定性随机数
    indcpa_enc(ct, msg, pk, seed); 
  }
  PRINT_AND_CSV("indcpa_enc: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    // ct:密文输出, key:共享密钥输出, pk:公钥, seed:确定性随机数
    crypto_kem_enc_derand(ct, key, pk, seed);
  }
  PRINT_AND_CSV("kem_enc_derand: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    crypto_kem_keypair(pk, sk);
  }
  PRINT_AND_CSV("keypair: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    crypto_kem_enc(ct, key, pk);
  }
  PRINT_AND_CSV("encaps: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    crypto_kem_dec(key, ct, sk);
  }
  PRINT_AND_CSV("decaps: ", t, NTESTS);

  return 0;
}