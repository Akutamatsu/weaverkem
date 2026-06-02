#include <stdio.h>
#include <stdint.h>

#define WEAVER_Q 3329

static uint32_t compress_q(uint32_t x, int d) {
    uint32_t num_buckets = 1u << d;
    return (uint32_t)(((uint64_t)x * num_buckets + WEAVER_Q / 2) / WEAVER_Q) & (num_buckets - 1);
}

int main() {
    int d = 9;
    uint32_t num_buckets = 1u << d;
    uint16_t count[4096] = {0};
    uint32_t first[4096];
    uint32_t lo0 = 0;

    for(uint32_t y = 0; y < num_buckets; y++) first[y] = WEAVER_Q;

    for(uint32_t x = 0; x < WEAVER_Q; x++) {
        uint32_t y = compress_q(x, d);
        if(first[y] == WEAVER_Q) first[y] = x;
        count[y]++;
    }

    for(uint32_t x = WEAVER_Q; x > 0; x--) {
        if(compress_q(x - 1, d) == 0) lo0 = x - 1;
        else break;
    }
    first[0] = lo0;

    printf("/* \n * 自动生成的 WEAVER-Inv 静态查找表 (d=%d) \n", d);
    printf(" * 仅包含 Lemire 拒绝采样所需的核心数据 \n */\n\n");
    
    printf("const invq_table_t invq_pk_table = {\n");
    
    printf("  .bucket_lo = {\n    ");
    for(uint32_t y = 0; y < num_buckets; y++) {
        printf("%4u", first[y]);
        if (y < num_buckets - 1) printf(",");
        if ((y + 1) % 16 == 0 && y < num_buckets - 1) printf("\n    ");
    }
    printf("\n  },\n");

    printf("  .bucket_size = {\n    ");
    for(uint32_t y = 0; y < num_buckets; y++) {
        printf("%3u", count[y]);
        if (y < num_buckets - 1) printf(",");
        if ((y + 1) % 16 == 0 && y < num_buckets - 1) printf("\n    ");
    }
    printf("\n  }\n");
    printf("};\n");

    return 0;
}