# WeaverKEM

本文件说明当前版本相对上一版提交zip的改动。

## 一、代码改动

### 1. CBD 采样常数时间化

> "Reference cbd.c popcount_u32 (eta=7,9) is a data-dependent loop"

`cbd.c` 中用于 η=7 / η=9 采样的 popcount 采用 `while(x) { r += x & 1u; x >>= 1; }`，
循环次数依赖秘密值，属于时序侧信道（非常数时间）。

改动：将 `popcount_u32`（`cbd.c`）与 `popcount_u32_scalar`（`cbd_avx2.c`）统一替换为SWAR 常数时间实现（固定位运算，无分支、无循环、无查表）。

范围：
- `Implementations/Reference_Implementation/WeaverKEM-{128,256,512}/cbd.c`
- `Implementations/Optimized_Implementation/WeaverKEM-{128,256,512}/cbd.c` 与 `cbd_avx2.c`
- 开发树 `ref/src` 与 `avx2_weaver/src`（含 `test_cbd.c`）


### 2. BCH 解码器缺陷修复

以下代码缺陷已完成修复：

1.WeaverKEM-256（`WEAVER_MODE==3`）中 `poly_tomsg` 缺失高层 BCH 解码，mode==3 分支恢复调用 `decode_bch_high_nibbles`（`msgenc.c`）

2.Berlekamp–Massey 差异值求和上界截断（`for (j=1; j<=i+1; ...)` 应到 `deg(elp)`） | 改为 `for (j=1; j<=t && j<=2*i+2; ...)`（含常数时间路径 `bch_ct_impl.h`） |

上述修复覆盖 `ref`、`avx2_weaver`、`ConstantTimeRef` 开发树，以及全部
Reference / Optimized 提交实例的 `msgenc.c` 与 12 个 `bch_high.c` / `bch_low.c`。

---
## 二、论文表述

以下条目属于论文/规范文档与实现的表述不一致，或工程实现细节。处理方式见对应 PDF 论文修订或下方说明：

- **Table 3 笔误**（"BCH(63,59,4)" 实为 (63,39,4) 缩短码）——已在论文中修正；
- **矩阵解析采样描述**（16-bit Lemire vs 13-bit 拒绝采样）——参考实现为 13-bit 拒绝采样、与规范一致；优化实现的 Lemire 说明已补充；
- **PRF nonce 编号** 与 `Test_Vectors`——`Test_Vectors` 已按当前代码重新生成并逐字节一致；
- **DFR 独立性假设**、**Theorem A.8 适用条件**——见论文修订；
- **参考实现其余常数时间项**（秘密相关除法等）——本次已处理 CBD 采样；其余加固见 `ConstantTimeRef/` 开发树；
- **Weaver-2048 的 SM3 占位强度封顶**——论文已明确该实例化为非规范性（normative 实例化请使用强度匹配的 XOF）。
- **WeaverKEM-256 Layer 2 DFR 重新测试结果为 -231.11

Weaver128L1 failure: 2.424e-19 = 2^-61.84
  BCH: (127, 113, 2), encoded_bits=126, payload_bits=112
  After correcting 2 bits from total 126, failure prob is 2^-167.17
  Public key size: 752
  Ciphertext size: 816

Weaver128L2 failure: 1.427e-18 = 2^-59.28
  BCH: (31, 21, 2), encoded_bits=26, payload_bits=16
  After correcting 2 bits from total 26, failure prob is 2^-166.33
  Public key size: 752
  Ciphertext size: 816

Weaver256L1 failure: 8.983e-17 = 2^-53.31
  BCH: (255, 223, 4), encoded_bits=252, payload_bits=220
  After correcting 4 bits from total 252, failure prob is 2^-233.55
  Public key size: 1312
  Ciphertext size: 1536

Weaver256L2 failure: 5.288e-16 = 2^-50.75
  BCH: (63, 39, 4), encoded_bits=60, payload_bits=36
  After correcting 4 bits from total 60, failure prob is 2^-231.11
  Public key size: 1312
  Ciphertext size: 1536

Weaver512L1 failure: 2.407e-21 = 2^-68.49
  BCH: (511, 448, 7), encoded_bits=511, payload_bits=448
  After correcting 7 bits from total 511, failure prob is 2^-491.27
  Public key size: 2848
  Ciphertext size: 3392

Weaver512L2 failure: 1.400e-20 = 2^-65.95
  BCH: (127, 78, 7), encoded_bits=113, payload_bits=64
  After correcting 7 bits from total 113, failure prob is 2^-488.36
  Public key size: 2848
  Ciphertext size: 3392

---

## 三、提交包内容

正式提交包含以下内容：

```
<submission-root>/
├── Implementations/
│   ├── Reference_Implementation/     # 可移植参考实现（ISO C），3 个实例
│   ├── Optimized_Implementation/     # AVX2 优化实现，3 个实例
│   └── README.md                     # 构建与 KAT 验证指南
├── Test_Vectors/
│   └── KAT_KEM_WeaverKEM-{128,256,512}.txt
└── NGCC_KEM (10).pdf                 # 算法规范文档
```

