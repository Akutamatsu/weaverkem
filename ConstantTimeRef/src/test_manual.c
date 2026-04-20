#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "params.h"
#include "kem.h"
#include "rng.h"

// 在 ref/correct 目录下
// # 假设你想测试 Level 1 (K=2)
// gcc -g -march=native -DKYBER_K=2 -o test_manual ../src/test_manual.c ../src/cbd.c ../src/fips202.c ../src/indcpa.c ../src/kem.c ../src/ntt.c ../src/poly.c ../src/polyvec.c ../src/reduce.c ../src/rng.c ../src/verify.c ../src/symmetric-shake.c -lcrypto -I../src

// # 运行它
// ./test_manual


// 辅助函数：把字节数组打印成 16 进制字符串
void print_hex(const char *label, const unsigned char *data, size_t len) {
    printf("%s: ", label);
    for(size_t i = 0; i < len; i++) {
        printf("%02X", data[i]);
    }
    printf("\n");
}

int main() {
    unsigned char pk[KYBER_PUBLICKEYBYTES];
    unsigned char sk[KYBER_SECRETKEYBYTES];
    unsigned char ct[KYBER_CIPHERTEXTBYTES];
    unsigned char ss_alice[KYBER_SSBYTES];
    unsigned char ss_bob[KYBER_SSBYTES];

    // 1. 初始化随机数发生器 (仅用于测试)
    unsigned char entropy_input[48];
    for (int i=0; i<48; i++) entropy_input[i] = i;
    randombytes_init(entropy_input, NULL, 256);

    printf("========== KEM 手动直观测试 ==========\n");
    printf("当前参数集多项式维度 N = %d\n\n", KYBER_N);

    // 2. Bob 生成公钥和私钥
    printf("[1] Bob 正在生成密钥对...\n");
    crypto_kem_keypair(pk, sk);
    printf("    -> 公钥生成完毕，准备发送给 Alice。\n\n");

    // 3. Alice 收到公钥，进行密钥封装
    printf("[2] Alice 使用 Bob 的公钥进行封装...\n");
    crypto_kem_enc(ct, ss_alice, pk);
    print_hex("    -> Alice 生成的共享密钥 (ss_alice)", ss_alice, KYBER_SSBYTES);
    printf("    -> 密文已生成，准备发送给 Bob。\n\n");

    // 4. Bob 收到密文，进行解封装
    printf("[3] Bob 收到密文，使用私钥进行解封装...\n");
    crypto_kem_dec(ss_bob, ct, sk);
    print_hex("    -> Bob 解出的共享密钥   (ss_bob)  ", ss_bob, KYBER_SSBYTES);
    printf("\n");

    // 5. 验证结果
    if (memcmp(ss_alice, ss_bob, KYBER_SSBYTES) == 0) {
        printf("✅ 测试成功！Alice 和 Bob 的共享密钥完全一致！\n");
    } else {
        printf("❌ 测试失败！密钥不匹配。\n");
    }
    printf("======================================\n");

    return 0;
}