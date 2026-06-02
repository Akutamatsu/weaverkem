#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "params.h"
#include "poly.h"
#include "msgenc.h"

static void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++)
        printf("%02X", data[i]);
    printf("\n");
}

int main(void) {
    uint8_t original_msg[WEAVER_INDCPA_MSGBYTES];
    uint8_t recovered_msg[WEAVER_INDCPA_MSGBYTES];
    poly p;
    int fail = 0;

    printf("=== WEAVER Codec 闭环及纠错能力测试 ===\n");
    printf("WEAVER_MODE = %d\n", WEAVER_MODE);
    printf("N = %d, MSG_BYTES = %d, Q = %d\n\n", WEAVER_N, WEAVER_INDCPA_MSGBYTES, WEAVER_Q);

    for (int i = 0; i < WEAVER_INDCPA_MSGBYTES; i++)
        original_msg[i] = (uint8_t)(i + 1);

    print_hex("[原始消息]", original_msg, WEAVER_INDCPA_MSGBYTES);

    poly_frommsg(&p, original_msg);
    poly_tomsg(recovered_msg, &p);
    if (memcmp(original_msg, recovered_msg, WEAVER_INDCPA_MSGBYTES) != 0) {
        printf("\n❌ 无噪声闭环失败\n");
        return 1;
    }
    printf("\n>> [0] 无噪声闭环 OK\n");

    srand((unsigned)time(NULL));

    poly_frommsg(&p, original_msg);
    printf("\n>> [1] poly_frommsg 编码完成.\n");

    const int num_errors = 4;
    printf(">> [2] 正在向多项式注入 %d 个系数级 Q/2 噪声...\n", num_errors);

    for (int i = 0; i < num_errors; i++) {
        int err_idx = rand() % WEAVER_N;
        int16_t before = p.coeffs[err_idx];
        p.coeffs[err_idx] = (int16_t)(p.coeffs[err_idx] + (WEAVER_Q / 2));
        printf("   - [破坏] 系数 @ 索引 %3d: %6d -> %6d\n",
               err_idx, before, p.coeffs[err_idx]);
    }

    poly_tomsg(recovered_msg, &p);
    printf("\n>> [3] poly_tomsg 解码完成.\n");
    print_hex("[恢复消息]", recovered_msg, WEAVER_INDCPA_MSGBYTES);

    if (memcmp(original_msg, recovered_msg, WEAVER_INDCPA_MSGBYTES) == 0) {
        printf("\n✅ 测试通过！纠错成功。\n");
    } else {
        printf("\n❌ 测试失败！\n");
        fail = 1;
    }

    return fail;
}

// gcc -O3 -march=native -DWEAVER_MODE=3 -I./src ./src/test_coding.c \
//     ./src/indcpa.c ./src/poly.c ./src/ntt.c ./src/cbd.c ./src/reduce.c ./src/polyvec.c \
//     ./src/bch_high.c ./src/bch_low.c ./src/msgenc.c ./src/fips202.c ./src/symmetric-shake.c \
//     ./src/poly_invq.c ./src/rng.c -lcrypto -o test_coding
