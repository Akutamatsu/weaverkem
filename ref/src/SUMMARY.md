# WEAVER-KEM 两个测试任务总结

---

## 任务一：密文压缩与解压缩

### 修改的代码

**1. params.h**

WEAVER-2048 的 dv 从 4 改为 6：

```c
// WEAVER-2048 修改前
#define KYBER_POLYCOMPRESSEDBYTES ((KYBER_N * 4) / 8)  // 256 bytes

// WEAVER-2048 修改后
#define KYBER_POLYCOMPRESSEDBYTES ((KYBER_N * 6) / 8)  // 384 bytes
```

**2. poly.c**

新增 6-bit 压缩分支（poly_compress）：

```c
#elif (KYBER_POLYCOMPRESSEDBYTES == (KYBER_N * 6 / 8))
  for(i=0; i<KYBER_N/4; i++) {
    for(j=0; j<4; j++)
      t[j] = ((((uint32_t)a->coeffs[4*i+j] << 6) + KYBER_Q/2)/KYBER_Q) & 63;
    r[0] = (t[0]>>0) | (t[1]<<6);
    r[1] = (t[1]>>2) | (t[2]<<4);
    r[2] = (t[2]>>4) | (t[3]<<2);
    r += 3;
  }
```

新增 6-bit 解压缩分支（poly_decompress）：

```c
#elif (KYBER_POLYCOMPRESSEDBYTES == (KYBER_N * 6 / 8))
  unsigned int j;
  uint8_t t[4];
  for(i=0; i<KYBER_N/4; i++) {
    t[0] = (a[0]>>0);              t[1] = (a[0]>>6) | (a[1]<<2);
    t[2] = (a[1]>>4) | (a[2]<<4); t[3] = (a[2]>>2);
    a += 3;
    for(j=0; j<4; j++)
      r->coeffs[4*i+j] = ((uint32_t)(t[j]&63)*KYBER_Q + 32) >> 6;
  }
```

### 测试内容（test_compress.c）

对三组参数各执行 4 项测试：

| 测试 | 内容 | 通过条件 |
|------|------|---------|
| A | 系数 → Compress(dv位) → 字节打包 → 字节解包 → Decompress | max_err ≤ q/2^(dv+1) |
| B | 系数 → Compress(du位) → 字节打包 → 字节解包 → Decompress | max_err ≤ q/2^(du+1) |
| C | 边界值：x=0 和 x=q-1 | 零值保持，误差在范围内 |
| D | 幂等性（论文正确性标准） | Compress(Decompress(Compress(x,d),d),d) == Compress(x,d) |

额外测试调用实际 poly_compress/poly_decompress，打印压缩字节和系数对比。

### 参数表（Table 1）

| 实例 | n | dv | du | dt | max_err(dv) | max_err(du) |
|------|---|----|----|----|-----------|-----------|
| WEAVER-512 | 128 | 4 | 9 | 9 | q/32 ≈ 104 | q/1024 ≈ 3 |
| WEAVER-1024 | 256 | 4 | 10 | 10 | q/32 ≈ 104 | q/2048 ≈ 1 |
| WEAVER-2048 | 512 | 6 | 10 | 10 | q/128 ≈ 26 | q/2048 ≈ 1 |

---

## 任务二：CBD 采样函数

### 修改的代码

**1. params.h**

```c
// WEAVER-512 修改前: ETA1=2  修改后:
#define KYBER_ETA1 4
#define KYBER_ETA2 4

// WEAVER-1024 修改前: ETA2=2  修改后:
#define KYBER_ETA1 2
#define KYBER_ETA2 4

// WEAVER-2048 修改前: ETA1=2, ETA2=2  修改后:
#define KYBER_ETA1 1
#define KYBER_ETA2 4
```

**2. cbd.c**

新增 cbd1()（η=1）：

```c
#if KYBER_ETA1 == 1 || KYBER_ETA2 == 1
static void cbd1(poly *r, const uint8_t buf[1*KYBER_N/4])
{
  unsigned int i,j;
  uint32_t t;
  for(i=0; i<KYBER_N/8; i++) {
    t = load32_littleendian(buf+4*i);
    for(j=0; j<8; j++) {
      int16_t a = (t >> (2*j+0)) & 0x1;
      int16_t b = (t >> (2*j+1)) & 0x1;
      r->coeffs[8*i+j] = a - b;   // range: {-1, 0, 1}
    }
  }
}
#endif
```

新增 cbd4()（η=4）：

```c
#if KYBER_ETA1 == 4 || KYBER_ETA2 == 4
static void cbd4(poly *r, const uint8_t buf[4*KYBER_N/4])
{
  unsigned int i,j;
  uint32_t t,d;
  for(i=0; i<KYBER_N/4; i++) {
    t = load32_littleendian(buf+4*i);
    d  = t & 0x11111111;  d += (t>>1) & 0x11111111;
    d += (t>>2) & 0x11111111;  d += (t>>3) & 0x11111111;
    for(j=0; j<4; j++) {
      int16_t a = (d >> (8*j+0)) & 0xf;
      int16_t b = (d >> (8*j+4)) & 0xf;
      r->coeffs[4*i+j] = a - b;   // range: {-4,...,4}
    }
  }
}
#endif
```

更新 cbd_eta1() 和 cbd_eta2()：

```c
void cbd_eta1(poly *r, const uint8_t buf[KYBER_ETA1*KYBER_N/4])
{
#if   KYBER_ETA1 == 1  
  cbd1(r, buf);
#elif KYBER_ETA1 == 2  
  cbd2(r, buf);
#elif KYBER_ETA1 == 3  
  cbd3(r, buf);
#elif KYBER_ETA1 == 4  
  cbd4(r, buf);
#endif
}

void cbd_eta2(poly *r, const uint8_t buf[KYBER_ETA2*KYBER_N/4])
{
#if   KYBER_ETA2 == 1  
  cbd1(r, buf);
#elif KYBER_ETA2 == 2  
  cbd2(r, buf);
#elif KYBER_ETA2 == 4  
  cbd4(r, buf);
#endif
}
```

### 测试内容（test_cbd.c）

对三组参数各执行 2 项测试：

| 测试 | 内容 | 通过条件 |
|------|------|---------|
| eta1 | 确定性输入 → CBD_eta1 采样 → 统计输出 | 所有系数 ∈ [-η₁, η₁] |
| eta2 | 确定性输入 → CBD_eta2 采样 → 统计输出 | 所有系数 ∈ [-η₂, η₂] |

每个测试打印：均值、方差、最小值、最大值、零/正/负系数个数。

额外测试调用实际 cbd_eta1/cbd_eta2（当前 WEAVER_MODE）。

### 参数表（Table 1）

| 实例 | n | η₁ | η₂ | 系数范围 η₁ | 系数范围 η₂ |
|------|---|----|----|-----------|----------|
| WEAVER-512 | 128 | 4 | 4 | [-4, 4] | [-4, 4] |
| WEAVER-1024 | 256 | 2 | 4 | [-2, 2] | [-4, 4] |
| WEAVER-2048 | 512 | 1 | 4 | [-1, 1] | [-4, 4] |
