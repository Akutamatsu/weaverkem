#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "params.h"
#include "poly.h"
#include "msgenc.h"
#include "bch.h"
// 打印字节的辅助函数
void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for(size_t i = 0; i < len; i++) {
        printf("%02X", data[i]);
    }
    printf("\n");
}

int main() {
    printf("=== 极简底层 Codec 闭环测试 ===\n");
    printf("%d\n",WEAVER_MODE);
    printf("N = %d, MSG_BYTES = %d\n\n", WEAVER_N, WEAVER_INDCPA_MSGBYTES);

    uint8_t original_msg[WEAVER_INDCPA_MSGBYTES];
    uint8_t recovered_msg[WEAVER_INDCPA_MSGBYTES];
    poly p;

    // 1. 构造一个测试明文 (例如: 01 02 03 ...)
    for(int i = 0; i < WEAVER_INDCPA_MSGBYTES; i++) {
        original_msg[i] = i + 1; 
    }

    print_hex("[原始消息]", original_msg, WEAVER_INDCPA_MSGBYTES);

    // 2. 编码 (Msg -> Poly)
    poly_frommsg(&p, original_msg);
    printf(">> poly_frommsg 编码完成.\n");

    // 3. 解码 (Poly -> Msg)  【注: 这里我们不加任何噪声】
    poly_tomsg(recovered_msg, &p);
    printf(">> poly_tomsg 解码完成.\n");

    print_hex("[恢复消息]", recovered_msg, WEAVER_INDCPA_MSGBYTES);

    // 4. 比对结果
    if(memcmp(original_msg, recovered_msg, WEAVER_INDCPA_MSGBYTES) == 0) {
        printf("\n✅ 测试通过！编解码器自身逻辑完美闭环！\n");
        printf("结论：错误发生在后续的 NTT、压缩阶段，或者是外层的宏定义(WEAVER_SYMBYTES)冲突。\n");
    } else {
        printf("\n❌ 测试失败！编解码器内部存在 Bug。\n");
        printf("结论：我们需要仔细检查 poly.c 里的位操作和 BCH 调用。\n");
    }

    return 0;
}

// gcc -O3 -march=native   -DWEAVER_MODE=1   -I./src   ./src/test_coding.c   ./src/indcpa.c   ./src/poly.c   ./src/ntt.c   ./src/cbd.c   ./src/reduce.c   ./src/polyvec.c   ./src/bch_high.c   ./src/bch_low.c   ./src/msgenc.c   ./src/fips202.c   ./src/symmetric-shake.c   -o test_coding