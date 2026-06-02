/*
 * 固定 derand 输入下输出 pk/sk/ct/ss，用于 ref 与 ref_ct_timing_fix 字节级对比。
 * 编译: -DWEAVER_MODE=1|3|5 ，且与其它 kem 对象一起链接（见 Makefile 同源列表）。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "api.h"
#include "kem.h"
#include "params.h"

static void dump_line(const char *name, const unsigned char *p, size_t n)
{
    size_t i;
    printf("%s=", name);
    for (i = 0; i < n; i++) {
        printf("%02X", p[i]);
    }
    printf("\n");
}

int main(void)
{
    unsigned char kp_coins[2 * WEAVER_SYMBYTES];
    unsigned char enc_coins[WEAVER_INDCPA_MSGBYTES];
    unsigned char pk[CRYPTO_PUBLICKEYBYTES];
    unsigned char sk[CRYPTO_SECRETKEYBYTES];
    unsigned char ct[CRYPTO_CIPHERTEXTBYTES];
    unsigned char ss[CRYPTO_BYTES];
    unsigned char ss_dec[CRYPTO_BYTES];
    size_t i;
    int r;

    for (i = 0; i < sizeof(kp_coins); i++) {
        kp_coins[i] = (unsigned char)((i * 17 + 3) ^ 0xA5u);
    }
    for (i = 0; i < sizeof(enc_coins); i++) {
        enc_coins[i] = (unsigned char)((i * 131 + 7) ^ 0x5Au);
    }

#if WEAVER_MODE == 1
    printf("ALG=WEAVER-512\n");
#elif WEAVER_MODE == 3
    printf("ALG=WEAVER-1024\n");
#elif WEAVER_MODE == 5
    printf("ALG=WEAVER-2048\n");
#else
    printf("ALG=UNKNOWN\n");
#endif

    if (crypto_kem_keypair_derand(pk, sk, kp_coins) != 0) {
        printf("ERR=keypair\n");
        return 2;
    }
    if (crypto_kem_enc_derand(ct, ss, pk, enc_coins) != 0) {
        printf("ERR=enc\n");
        return 3;
    }
    if (crypto_kem_dec(ss_dec, ct, sk) != 0) {
        printf("ERR=dec\n");
        return 4;
    }

    dump_line("pk", pk, sizeof pk);
    dump_line("sk", sk, sizeof sk);
    dump_line("ct", ct, sizeof ct);
    dump_line("ss", ss, sizeof ss);
    dump_line("ss_dec", ss_dec, sizeof ss_dec);

    r = memcmp(ss, ss_dec, CRYPTO_BYTES);
    printf("ss_match=%d\n", r == 0 ? 1 : 0);
    return r == 0 ? 0 : 1;
}
