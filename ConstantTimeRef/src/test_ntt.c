#include <stdio.h>
#include <stdlib.h>
#include "params.h"
#include "poly.h"
#include "ntt.h"
#include "reduce.h"

int main() {
    int16_t a[KYBER_N];
    int16_t b[KYBER_N];
    
    // 1. 初始化一个随机多项式
    for(int i = 0; i < KYBER_N; i++) {
        a[i] = rand() % KYBER_Q;
        b[i] = a[i]; // 备份
    }

    // 2. 进行正向和逆向 NTT
    ntt(b);
    invntt(b);

    // 3. 验证结果是否一致
    int success = 1;
    for(int i = 0; i < KYBER_N; i++) {
        // 【关键修复】: 消除 Kyber 在 invntt 结尾附加的 Montgomery 因子 (R)
        // montgomery_reduce(x * 1) 相当于计算 (x * 1 * R^-1) mod Q
        int16_t restored = montgomery_reduce((int32_t)b[i] * 1);
        
        // 映射到正数区间 [0, Q-1] 以便比较
        int16_t expected = a[i] % KYBER_Q;
        if (expected < 0) expected += KYBER_Q;
        
        int16_t actual = restored % KYBER_Q;
        if (actual < 0) actual += KYBER_Q;
        
        if(expected != actual) {
            printf("错误! 索引 %d: 期望 %d, 实际得到 %d (未还原前的裸数据是 %d)\n", i, expected, actual, b[i]);
            success = 0;
            break;
        }
    }

    if(success) {
        printf("恭喜！NTT 和 INTT 数学逻辑完全正确！\n");
    }
    return 0;
}