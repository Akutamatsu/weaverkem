# WeaverKEM — KAT Testing Guide

本文档说明如何在 **Unix/Linux** 和 **Windows (MSYS2 MINGW64)** 上编译并测试 Reference 与 Optimized 实现，并验证测试向量的正确性。

---

## 目录结构

解压提交压缩包后，目录结构如下：

```text
<submission-root>/
├── Implementations/
│   ├── Reference_Implementation/
│   │   ├── WeaverKEM-128/                # WEAVER_MODE=1，128-bit 经典安全
│   │   ├── WeaverKEM-256/                # WEAVER_MODE=3，256-bit 经典安全
│   │   └── WeaverKEM-512/                # WEAVER_MODE=5，512-bit 经典安全
│   └── Optimized_Implementation/         # AVX2 优化实现（需 OpenSSL）
│       ├── WeaverKEM-128/
│       ├── WeaverKEM-256/
│       └── WeaverKEM-512/
└── Test_Vectors/
    ├── KAT_KEM_WeaverKEM-128.txt
    ├── KAT_KEM_WeaverKEM-256.txt
    └── KAT_KEM_WeaverKEM-512.txt
```

验证 diff 命令以 **`<submission-root>/`** 为工作目录；编译前请先确认当前目录正确：

```bash
cd ~/weaverkem    # 替换为你的项目根目录
ls Implementations Test_Vectors   # 两个目录都应存在
```

---

## 方法一：Unix / Linux 测试（推荐）

### 前置条件

**Reference Implementation：**

```bash
# Ubuntu / Debian
sudo apt update && sudo apt install -y gcc cmake make

# CentOS / RHEL / Fedora
sudo yum install -y gcc cmake make
```

**Optimized Implementation**（额外需要 OpenSSL）：

```bash
# Ubuntu / Debian
sudo apt install -y libssl-dev
```

验证工具链：

```bash
gcc --version
cmake --version
```

### Reference：编译并生成测试向量

```bash
cd ~/weaverkem/Implementations/Reference_Implementation
mkdir -p build && cd build
cmake ..
cmake --build . --target generate_kat
```

如需从头重新配置：

```bash
cd ..
rm -rf build
```

构建成功时输出三行：

```text
Files have been saved in the 'output' folder within the working directory.
```

生成的 `.txt` 文件位于 `Implementations/Reference_Implementation/build/bin/output/`。

### Optimized：编译并生成测试向量

```bash
cd ~/weaverkem/Implementations/Optimized_Implementation
mkdir -p build && cd build
cmake ..
cmake --build . --target generate_kat
```

生成的 `.txt` 文件位于 `Implementations/Optimized_Implementation/build/bin/output/`。

### 验证 output 与 Test_Vectors 完全一致

在 **`~/weaverkem/`**（项目根目录）下执行：

```bash
cd ~/weaverkem
```

**Reference：**

```bash
for NAME in WeaverKEM-128 WeaverKEM-256 WeaverKEM-512; do
    FILE1="Implementations/Reference_Implementation/build/bin/output/KAT_KEM_$NAME.txt"
    FILE2="Test_Vectors/KAT_KEM_$NAME.txt"
    if diff -q "$FILE1" "$FILE2" > /dev/null 2>&1; then
        echo "$NAME: PASS (output == Test_Vectors)"
    else
        echo "$NAME: FAIL (files differ)"
    fi
done
```

**Optimized（手动 diff）：**

```bash
for NAME in WeaverKEM-128 WeaverKEM-256 WeaverKEM-512; do
    FILE1="Implementations/Optimized_Implementation/build/bin/output/KAT_KEM_$NAME.txt"
    FILE2="Test_Vectors/KAT_KEM_$NAME.txt"
    if diff -q "$FILE1" "$FILE2" > /dev/null 2>&1; then
        echo "$NAME: PASS (output == Test_Vectors)"
    else
        echo "$NAME: FAIL (files differ)"
    fi
done
```

**Optimized（CMake 一键验证）：**

```bash
cd ~/weaverkem/Implementations/Optimized_Implementation/build
cmake --build . --target verify_kat
```

预期输出（三组均 PASS）：

```text
WeaverKEM-128: PASS (output == Test_Vectors)
WeaverKEM-256: PASS (output == Test_Vectors)
WeaverKEM-512: PASS (output == Test_Vectors)
```

---

## 方法二：Windows MSYS2 MINGW64 测试

> **重要**：必须使用 **MSYS2 MINGW64 终端**（标题栏显示 `MINGW64`），
> 不能使用 PowerShell 或 cmd.exe。

### 前置条件

```bash
pacman -S mingw-w64-x86_64-gcc make cmake
# Optimized 还需：
pacman -S mingw-w64-x86_64-openssl
```

### 编译与验证

流程与 Linux 相同，使用 `cmake --build . --target generate_kat` 替代 `make generate_kat` 即可（两者在 MSYS2 中均可使用）。

请确保路径不含特殊字符、中文或空格。

---

## 测试结果解读

| 程序输出 | 含义 |
|----------|------|
| `Files have been saved in the 'output' folder` | 编译成功，10 组 encap/decap 自验证全部通过 |
| `ERROR: decapsulation shared secret key != ...` | 解封装结果不一致，算法实现有误 |
| `ERROR: kem_keygen returned ...` | 密钥生成失败 |
| diff 无输出（PASS） | output 与 Test_Vectors 字节完全一致，可复现性验证通过 |
| diff 有差异（FAIL） | 若实现修复后产生了新的正确 KAT，应同步用 output 覆盖更新 Test_Vectors 后再提交 |

---

## 预期参数尺寸

编译运行后，测试向量文件中每组数据的字段长度应与下表完全一致：

| 实例 | PK (bytes) | SK (bytes) | CT (bytes) | SS (bytes) |
|------|------------|------------|------------|------------|
| WeaverKEM-128 | 752 | 1776 | 816 | 16 |
| WeaverKEM-256 | 1312 | 3072 | 1536 | 32 |
| WeaverKEM-512 | 2880 | 6400 | 3392 | 64 |
