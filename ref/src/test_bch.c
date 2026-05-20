/* BCH 纠错自测：高位 + 低位（模式 3/5 才有低位实现）
 *
 * 说明：通过本程序不能数学上“证明”BCH 实现完全正确；只能做有界回归。
 * 补强：在「仅污染数据、校验按原文接收」时，译码后须满足
 *       encode(纠错后载荷) == recv_ecc（码字闭合），否则判 FAIL。
 *
 * WEAVER-512（仅高位）:
 *   gcc -O2 -Wall -Wno-unused-variable -DWEAVER_MODE=1 -o test_bch \\
 *       test_bch.c bch_high.c
 *
 * WEAVER-1024 / 2048（高位 + 低位，须同时链接 bch_low.c）:
 *   gcc -O2 -Wall -Wno-unused-variable -DWEAVER_MODE=3 -o test_bch \\
 *       test_bch.c bch_high.c bch_low.c
 *   （将 WEAVER_MODE=3 改为 5 可测 2048 档；低位在 1024 为半字节 API，2048 为字节 API）
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "params.h"
#include "bch.h"

#if WEAVER_MODE == 1
#  define BCH_T_HIGH 5
#  define DATA_LEN_HIGH 16
#  define ECC_BYTES_HIGH 5
#elif WEAVER_MODE == 3
#  define BCH_T_HIGH 4
#  define DATA_LEN_HIGH 27
#  define ECC_BYTES_HIGH 4
#  define BCH_T_LOW 4
#  define LOW_NIBBLES 9 /* msgenc: ELL_DDOT_NIBBLES */
#  define LOW_BUF_BYTES ((LOW_NIBBLES + 1) / 2)
#  define ECC_BYTES_LOW 3
#elif WEAVER_MODE == 5
#  define BCH_T_HIGH 6
#  define DATA_LEN_HIGH 57
#  define ECC_BYTES_HIGH 7
#  define BCH_T_LOW 6
#  define DATA_LEN_LOW 7  /* msgenc: ELL_DDOT_BYTES */
#  define ECC_BYTES_LOW 6 /* msgenc: LOW_ECC_BYTES */
#else
#  error "WEAVER_MODE must be 1, 3, or 5"
#endif

static void flip_bit(uint8_t *buf, unsigned bit)
{
    buf[bit / 8] ^= (uint8_t)(1u << (bit % 8));
}

/* MSB-first bit order (matches msgenc / decode_bch_* correction) */
static void flip_bit_msb(uint8_t *buf, unsigned bit)
{
    buf[bit / 8] ^= (uint8_t)(1u << (7 - (bit % 8)));
}

static void print_hex(const char *label, const uint8_t *p, size_t n)
{
    fputs(label, stdout);
    for (size_t i = 0; i < n; i++)
        printf("%02x%c", p[i], (i + 1 < n) ? ' ' : '\n');
}

#if WEAVER_MODE == 3
static int same_low_nibbles(const uint8_t *a, const uint8_t *b, unsigned nibbles)
{
    unsigned nb = (nibbles + 1) / 2;
    if (memcmp(a, b, nb - (nibbles & 1u ? 1u : 0)) != 0)
        return 0;
    if (nibbles & 1u)
        return ((a[nb - 1] ^ b[nb - 1]) & 0xf0u) == 0;
    return 1;
}
#endif

static int test_bch_high(void)
{
    uint8_t gold[DATA_LEN_HIGH], data[DATA_LEN_HIGH];
    uint8_t ecc[ECC_BYTES_HIGH];
    int e, r;

    for (unsigned i = 0; i < DATA_LEN_HIGH; i++)
        gold[i] = (uint8_t)(0xA5 ^ (i * 17));

    printf("========== BCH high ==========\n");
    print_hex("原始载荷: ", gold, DATA_LEN_HIGH);

    for (e = 0; e <= BCH_T_HIGH; e++) {
        memcpy(data, gold, DATA_LEN_HIGH);
        encode_bch_high(data, DATA_LEN_HIGH, ecc);
        for (int k = 0; k < e; k++)
            flip_bit(data, (unsigned)(k * 13 + 3));

        printf("\n【高位 注入 %d 比特错】\n", e);
        print_hex("  翻转后: ", data, DATA_LEN_HIGH);

        r = decode_bch_high(data, DATA_LEN_HIGH, ecc);
        printf("  decode_bch_high 返回: %d\n", r);
        print_hex("  纠错后: ", data, DATA_LEN_HIGH);

        if (r != e || memcmp(data, gold, DATA_LEN_HIGH) != 0) {
            printf("FAIL high: %d err, ret=%d\n", e, r);
            return 1;
        }
        {
            uint8_t ecc_chk[ECC_BYTES_HIGH];
            encode_bch_high(data, DATA_LEN_HIGH, ecc_chk);
            if (memcmp(ecc_chk, ecc, sizeof ecc_chk) != 0) {
                printf("FAIL high: codeword parity mismatch after decode (e=%d)\n",
                       e);
                return 1;
            }
        }
    }

    memcpy(data, gold, DATA_LEN_HIGH);
    encode_bch_high(data, DATA_LEN_HIGH, ecc);
    for (int k = 0; k <= BCH_T_HIGH; k++)
        flip_bit(data, (unsigned)(k * 11 + 1));

    printf("\n【高位 注入 t+1 = %d 比特错】\n", BCH_T_HIGH + 1);
    print_hex("  翻转后: ", data, DATA_LEN_HIGH);
    r = decode_bch_high(data, DATA_LEN_HIGH, ecc);
    printf("  decode_bch_high 返回: %d\n", r);
    print_hex("  纠错后: ", data, DATA_LEN_HIGH);
    printf("  与原始载荷一致: %s\n",
           memcmp(data, gold, DATA_LEN_HIGH) == 0 ? "是" : "否");

    if (memcmp(data, gold, DATA_LEN_HIGH) == 0) {
        printf("FAIL high: t+1 restored payload (ret=%d)\n", r);
        return 1;
    }

#if WEAVER_MODE == 5
    {
        static const unsigned stress_bits[] = {3, 250, 256, 300, 400, 436, 450, 0};
        printf("\n【高位 MSB 单比特压力测试 (mode 5)】\n");
        for (int k = 0; stress_bits[k] || k == 0; k++) {
            unsigned bi = stress_bits[k];
            if (bi == 0 && k > 0)
                break;
            memcpy(data, gold, DATA_LEN_HIGH);
            encode_bch_high(data, DATA_LEN_HIGH, ecc);
            flip_bit_msb(data, bi);
            r = decode_bch_high(data, DATA_LEN_HIGH, ecc);
            if (r != 1 || memcmp(data, gold, DATA_LEN_HIGH) != 0) {
                printf("FAIL high MSB flip bit %u: ret=%d\n", bi, r);
                return 1;
            }
            printf("  bit %3u OK\n", bi);
        }
    }
#endif

    printf("OK high (WEAVER_MODE=%d)\n", WEAVER_MODE);
    return 0;
}

#if WEAVER_MODE == 3 || WEAVER_MODE == 5
static int test_bch_low(void)
{
    int e, r;

    printf("\n========== BCH low ==========\n");

#  if WEAVER_MODE == 3
    {
        uint8_t gold[LOW_BUF_BYTES], data[LOW_BUF_BYTES];
        uint8_t ecc[ECC_BYTES_LOW];
        const unsigned msg_bits = LOW_NIBBLES * 4u;

        for (unsigned i = 0; i < LOW_BUF_BYTES; i++)
            gold[i] = (uint8_t)(0x5C ^ (i * 23));
        gold[LOW_BUF_BYTES - 1] &= 0xf0u; /* 仅高半字节属于第 9 个 nibble */

        print_hex("原始载荷 (9 nibbles): ", gold, LOW_BUF_BYTES);

        for (e = 0; e <= BCH_T_LOW; e++) {
            memcpy(data, gold, LOW_BUF_BYTES);
            encode_bch_low_nibbles(data, LOW_NIBBLES, ecc);
            for (int k = 0; k < e; k++)
                flip_bit(data, (unsigned)((k * 11 + 5) % msg_bits));

            printf("\n【低位 注入 %d 比特错】\n", e);
            print_hex("  翻转后: ", data, LOW_BUF_BYTES);

            r = decode_bch_low_nibbles(data, LOW_NIBBLES, ecc);
            printf("  decode_bch_low_nibbles 返回: %d\n", r);
            print_hex("  纠错后: ", data, LOW_BUF_BYTES);

            if (r != e || !same_low_nibbles(data, gold, LOW_NIBBLES)) {
                printf("FAIL low: %d err, ret=%d\n", e, r);
                return 1;
            }
            {
                uint8_t ecc_chk[ECC_BYTES_LOW];
                encode_bch_low_nibbles(data, LOW_NIBBLES, ecc_chk);
                if (memcmp(ecc_chk, ecc, sizeof ecc_chk) != 0) {
                    printf("FAIL low: parity mismatch after decode (e=%d)\n", e);
                    return 1;
                }
            }
        }

        memcpy(data, gold, LOW_BUF_BYTES);
        encode_bch_low_nibbles(data, LOW_NIBBLES, ecc);
        {
            uint8_t ecc_rx[ECC_BYTES_LOW];
            memcpy(ecc_rx, ecc, sizeof ecc_rx);
            ecc_rx[0] ^= 1u; /* 校验区 1 比特错 */
            for (int k = 0; k < BCH_T_LOW; k++)
                flip_bit(data, (unsigned)((k * 9 + 2) % msg_bits));
            printf("\n【低位 注入 t+1 = %d 比特错（t 数据 + 1 校验）】\n",
                   BCH_T_LOW + 1);
            print_hex("  翻转后: ", data, LOW_BUF_BYTES);
            r = decode_bch_low_nibbles(data, LOW_NIBBLES, ecc_rx);
            printf("  decode_bch_low_nibbles 返回: %d\n", r);
            print_hex("  纠错后: ", data, LOW_BUF_BYTES);
            printf("  与原始载荷一致: %s\n",
                   same_low_nibbles(data, gold, LOW_NIBBLES) ? "是" : "否");
            if (same_low_nibbles(data, gold, LOW_NIBBLES)) {
                printf("FAIL low: t+1 restored payload (ret=%d)\n", r);
                return 1;
            }
        }
    }
#  else /* WEAVER_MODE == 5 */
    {
        uint8_t gold[DATA_LEN_LOW], data[DATA_LEN_LOW];
        uint8_t ecc[ECC_BYTES_LOW];
        const unsigned msg_bits = DATA_LEN_LOW * 8u;

        for (unsigned i = 0; i < DATA_LEN_LOW; i++)
            gold[i] = (uint8_t)(0x3C ^ (i * 29));

        print_hex("原始载荷: ", gold, DATA_LEN_LOW);

        for (e = 0; e <= BCH_T_LOW; e++) {
            memcpy(data, gold, DATA_LEN_LOW);
            encode_bch_low(data, DATA_LEN_LOW, ecc);
            for (int k = 0; k < e; k++)
                flip_bit(data, (unsigned)((k * 13 + 7) % msg_bits));

            printf("\n【低位 注入 %d 比特错】\n", e);
            print_hex("  翻转后: ", data, DATA_LEN_LOW);

            r = decode_bch_low(data, DATA_LEN_LOW, ecc);
            printf("  decode_bch_low 返回: %d\n", r);
            print_hex("  纠错后: ", data, DATA_LEN_LOW);

            if (r != e || memcmp(data, gold, DATA_LEN_LOW) != 0) {
                printf("FAIL low: %d err, ret=%d\n", e, r);
                return 1;
            }
            {
                uint8_t ecc_chk[ECC_BYTES_LOW];
                encode_bch_low(data, DATA_LEN_LOW, ecc_chk);
                if (memcmp(ecc_chk, ecc, sizeof ecc_chk) != 0) {
                    printf("FAIL low: parity mismatch after decode (e=%d)\n", e);
                    return 1;
                }
            }
        }

        memcpy(data, gold, DATA_LEN_LOW);
        encode_bch_low(data, DATA_LEN_LOW, ecc);
        {
            uint8_t ecc_rx[ECC_BYTES_LOW];
            memcpy(ecc_rx, ecc, sizeof ecc_rx);
            ecc_rx[0] ^= 1u; /* 校验区 1 比特 */
            for (int k = 0; k < BCH_T_LOW; k++)
                flip_bit(data, (unsigned)((k * 11 + 3) % msg_bits));

            printf("\n【低位 注入 t+1 = %d 比特错（t 数据 + 1 校验）】\n",
                   BCH_T_LOW + 1);
            print_hex("  翻转后: ", data, DATA_LEN_LOW);
            r = decode_bch_low(data, DATA_LEN_LOW, ecc_rx);
            printf("  decode_bch_low 返回: %d\n", r);
            print_hex("  纠错后: ", data, DATA_LEN_LOW);
            printf("  与原始载荷一致: %s\n",
                   memcmp(data, gold, DATA_LEN_LOW) == 0 ? "是" : "否");
            if (memcmp(data, gold, DATA_LEN_LOW) == 0) {
                printf("FAIL low: t+1 restored payload (ret=%d)\n", r);
                return 1;
            }
        }
    }
#  endif

    printf("OK low (WEAVER_MODE=%d)\n", WEAVER_MODE);
    return 0;
}
#endif

int main(void)
{
    if (test_bch_high() != 0)
        return 1;
#if WEAVER_MODE == 3 || WEAVER_MODE == 5
    if (test_bch_low() != 0)
        return 1;
#endif
    printf("\n全部通过。\n");
    return 0;
}
