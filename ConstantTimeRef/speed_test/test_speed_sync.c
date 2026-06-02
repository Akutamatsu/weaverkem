/*
 * 与 compare_ref_impl 相同思路：固定 derand 输入，再跑 speed（避免 randombytes 每轮不同）。
 * 编译: -DWEAVER_MODE=1|3|5 ，与其余 kem 源同链接。
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "api.h"
#include "params.h"
#include "kem.h"
#include "cpucycles.h"
#include "speed_print.h"

#define NTESTS 10000

static uint64_t t[NTESTS];

static void fill_coins(uint8_t *kp_coins, uint8_t *enc_coins)
{
    size_t i;
    for (i = 0; i < 2u * WEAVER_SYMBYTES; i++) {
        kp_coins[i] = (uint8_t)((i * 17u + 3u) ^ 0xA5u);
    }
    for (i = 0; i < (size_t)WEAVER_INDCPA_MSGBYTES; i++) {
        enc_coins[i] = (uint8_t)((i * 131u + 7u) ^ 0x5Au);
    }
}

int main(void)
{
    unsigned int i;
    uint8_t kp_coins[2 * WEAVER_SYMBYTES];
    uint8_t enc_coins[WEAVER_INDCPA_MSGBYTES];
    uint8_t pk[CRYPTO_PUBLICKEYBYTES] = {0};
    uint8_t sk[CRYPTO_SECRETKEYBYTES] = {0};
    uint8_t ct[CRYPTO_CIPHERTEXTBYTES] = {0};
    uint8_t key[CRYPTO_BYTES] = {0};

    fill_coins(kp_coins, enc_coins);

    printf("%s start (sync derand)..\n", CRYPTO_ALGNAME);

    for (i = 0; i < NTESTS; i++) {
        t[i] = cpucycles();
        (void)crypto_kem_keypair_derand(pk, sk, kp_coins);
    }
    print_results("keypair_derand: ", t, NTESTS);

    for (i = 0; i < NTESTS; i++) {
        t[i] = cpucycles();
        (void)crypto_kem_enc_derand(ct, key, pk, enc_coins);
    }
    print_results("encaps_derand: ", t, NTESTS);

    for (i = 0; i < NTESTS; i++) {
        t[i] = cpucycles();
        (void)crypto_kem_dec(key, ct, sk);
    }
    print_results("decaps: ", t, NTESTS);

    return 0;
}
