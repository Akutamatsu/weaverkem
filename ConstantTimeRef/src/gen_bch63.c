#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// GF(2^6) 的指数表和对数表，大小为 64
uint16_t a_pow_tab[64] = {0};
uint16_t a_log_tab[64] = {0};

// GF(64) 乘法
int gf_mul(int a, int b) {
    if (a == 0 || b == 0) return 0;
    return a_pow_tab[(a_log_tab[a] + a_log_tab[b]) % 63];
}

int main() {
    // 1. 初始化 GF(2^6) 查找表
    // 使用标准本原多项式 x^6 + x + 1 (二进制 1000011 = 67)
    int primitive_poly = 67; 
    int val = 1;
    for (int i = 0; i < 63; i++) {
        a_pow_tab[i] = val;
        a_log_tab[val] = i;
        val <<= 1;
        // 如果溢出第 6 位 (即达到 64)，则异或本原多项式
        if (val & 64) {
            val ^= primitive_poly;
        }
    }
    a_pow_tab[63] = a_pow_tab[0]; // 补齐 64 长度
    a_log_tab[0] = 0;

    // 2. 计算 BCH(63, 45, 3) 的生成多项式 g(x)
    // t=3 需要的共轭根类
    int roots[] = {
        1, 2, 4, 8, 16, 32,             // C_1 (6 阶)
        3, 6, 12, 24, 48, 33,           // C_3 (6 阶)
        5, 10, 20, 40, 17, 34           // C_5 (6 阶)
    };
    int num_roots = 18; // 生成多项式阶数为 18 (ecc_bits)

    int g[19] = {0};
    g[0] = 1; 
    
    for (int i = 0; i < num_roots; i++) {
        int root = a_pow_tab[roots[i]];
        int next_g[19] = {0};
        for (int j = 0; j <= i + 1; j++) {
            int term1 = (j > 0) ? g[j - 1] : 0;     
            int term2 = gf_mul(g[j], root);         
            next_g[j] = term1 ^ term2;              
        }
        for (int j = 0; j <= i + 1; j++) {
            g[j] = next_g[j];
        }
    }

    // 3. 将 18 阶多项式左对齐到 32 位的最高有效位 (32 - 18 = 14 位偏移)
    uint32_t G = 0;
    for (int i = 0; i < 18; i++) {
        if (g[i]) {
            G |= (1U << (i + 14));
        }
    }

    // 4. 模拟并生成 LAC 所需的 4-bit 步进加速表
    uint32_t mod8_tab_half[16];
    for (int v = 0; v < 16; v++) {
        uint32_t state = ((uint32_t)v) << 28;
        for (int step = 0; step < 4; step++) { 
            if (state & 0x80000000) {
                state = (state << 1) ^ G;
            } else {
                state = (state << 1);
            }
        }
        mod8_tab_half[v] = state;
    }

    // =======================================================
    // 5. 写入头文件 (严格遵循防冲突和兼容性规范)
    // =======================================================
    FILE *f = fopen("bch63.h", "w");
    if (!f) return 1;

    fprintf(f, "#ifndef WEAVER_BCH63_H\n");
    fprintf(f, "#define WEAVER_BCH63_H\n\n");
    fprintf(f, "#include <stdint.h>\n");
    fprintf(f, "#include \"bch.h\"\n\n");

    // 加上 static const 和 bch63_ 前缀，防止与其他 BCH 冲突
    fprintf(f, "static const uint16_t bch63_a_pow_tab[64] = {");
    for(int i=0; i<64; i++) fprintf(f, "%d%s", a_pow_tab[i], i==63?"":",");
    fprintf(f, "};\n\n");

    fprintf(f, "static const int bch63_a_log_tab[64] = {");
    for(int i=0; i<64; i++) fprintf(f, "%d%s", a_log_tab[i], i==63?"":",");
    fprintf(f, "};\n\n");

    fprintf(f, "static const uint32_t bch63_mod8_tab_half[16] = {");
    for(int i=0; i<16; i++) fprintf(f, "%u%s", mod8_tab_half[i], i==15?"":",");
    fprintf(f, "};\n\n");

    // 实例化控制块
    fprintf(f, "static const struct bch_control bch63_ctrl = {\n");
    fprintf(f, "    .m = 6,\n");         // GF(2^6)
    fprintf(f, "    .t = 3,\n");         // 纠错能力
    fprintf(f, "    .n = 63,\n");        // 环长度
    fprintf(f, "    .ecc_bytes = 3,\n"); // 18 bits 需要 3 个字节来存放
    fprintf(f, "    .ecc_bits = 18,\n"); // 实际校验位长度
    fprintf(f, "    .ecc_words = 1,\n"); // 18 bits 能放入 1 个 32位字
    fprintf(f, "    .mod8_tab_half = bch63_mod8_tab_half,\n");
    fprintf(f, "    .a_pow_tab     = bch63_a_pow_tab,\n");
    fprintf(f, "    .a_log_tab     = (const int *)bch63_a_log_tab\n"); // 强转以消除指针类型警告
    fprintf(f, "};\n\n");

    fprintf(f, "#endif // WEAVER_BCH63_H\n");

    fclose(f);
    printf("成功生成 bch63.h 文件！\n");
    return 0;
}