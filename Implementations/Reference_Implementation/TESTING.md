# WeaverKEM Reference Implementation — Testing Guide

本文档说明如何在 **Unix/Linux** 和 **Windows (MSYS2 MINGW64)** 上编译并测试三个参考实现，并验证测试向量的正确性。

---

## 目录结构

解压提交压缩包后，目录结构如下：

```text
weaverkem-main/
├── ref/src/                              # 算法核心源码（三个实例共享）
├── Implementations/
│   └── Reference_Implementation/
│       ├── WeaverKEM-128/                # WEAVER_MODE=1，128-bit 经典安全
│       ├── WeaverKEM-256/                # WEAVER_MODE=3，256-bit 经典安全
│       └── WeaverKEM-512/                # WEAVER_MODE=5，512-bit 经典安全
└── Test_Vectors/
    ├── KAT_KEM_WeaverKEM-128.txt
    ├── KAT_KEM_WeaverKEM-256.txt
    └── KAT_KEM_WeaverKEM-512.txt
```

以下所有命令均以 **`weaverkem-main/` 为工作目录起点**。

---

## 方法一：Unix / Linux 测试（推荐，评审方使用此方式）

### 前置条件

```bash
# Ubuntu / Debian
sudo apt update && sudo apt install -y gcc make cmake

# CentOS / RHEL / Fedora
sudo yum install -y gcc make cmake
```

验证工具链已就绪：

```bash
gcc --version
cmake --version
```

### 编译并生成测试向量

进入 `weaverkem-main/Implementations/Reference_Implementation` 目录，执行以下命令：

```bash
mkdir build
cd build
cmake ..
make generate_kat
```

如需从头重新配置构建目录，以如下命令删除构建目录后，再重新执行上述命令即可：

```bash
cd ..
rm -rf build
```

构建成功时，将在`build/bin` 目录下生成三个 KAT 输出程序，分别对应三个安全级别并自动执行，三个程序都运行成功将输出三行：

```text
Files have been saved in the 'output' folder within the working directory.

Files have been saved in the 'output' folder within the working directory.

Files have been saved in the 'output' folder within the working directory.
```

程序执行的产物 .txt 文件将输出到 `build/bin/output` 路径下。 

### 验证 output 与 Test_Vectors 完全一致

在 `weaverkem-main/` 目录下执行：

```bash
for NAME in WeaverKEM-128 WeaverKEM-256 WeaverKEM-512; do
    FILE1="Implementations/Reference_Implementation/build/bin/output/KAT_KEM_$NAME.txt"
    FILE2="Test_Vectors/KAT_KEM_$NAME.txt"
    if diff -q "$FILE1" "$FILE2" > /dev/null 2>&1; then
        echo "$NAME : PASS (output == Test_Vectors)"
    else
        echo "$NAME : FAIL (files differ)"
    fi
done
```

预期输出：

```text
WeaverKEM-128 : PASS (output == Test_Vectors)
WeaverKEM-256 : PASS (output == Test_Vectors)
WeaverKEM-512 : PASS (output == Test_Vectors)
```

### 当 output 与 Test_Vectors 不一致时

若实现修复后生成了新的正确 KAT，应同步更新 `Test_Vectors/`：

```bash
cp Implementations/Reference_Implementation/WeaverKEM-128/output/KAT_KEM_WeaverKEM-128.txt Test_Vectors/KAT_KEM_WeaverKEM-128.txt
cp Implementations/Reference_Implementation/WeaverKEM-256/output/KAT_KEM_WeaverKEM-256.txt Test_Vectors/KAT_KEM_WeaverKEM-256.txt
cp Implementations/Reference_Implementation/WeaverKEM-512/output/KAT_KEM_WeaverKEM-512.txt Test_Vectors/KAT_KEM_WeaverKEM-512.txt
```

## 方法二：Windows MSYS2 MINGW64 测试

> **重要**：必须使用 **MSYS2 MINGW64 终端**（从开始菜单打开，标题栏显示 `MINGW64`），
> 不能使用 PowerShell 或 cmd.exe。
>
> 若项目路径**不含**中文或空格，可直接在各实例目录下使用 `make kat`，也可使用与上文 Unix/Linux 一致的 CMake 命令。
>
> 若项目路径**含有**中文或空格字符，请使用下方的绝对路径 gcc 命令（将 `<PROJECT_ROOT>`
> 替换为实际路径，格式为 MSYS2 风格，例如 `/c/Users/username/Desktop/weaverkem-main`）。

### 前置条件

在 MSYS2 MINGW64 终端中安装工具链：

```bash
pacman -S mingw-w64-x86_64-gcc make cmake
```

### 请确保路径不含特殊字符、中文或空格，然后...

使用 cmake 的流程和前面基本一致（除了新建目录与删除目录等命令外），在此不做赘述。



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
| WeaverKEM-128 | 608 | 1440 | 704 | 16 |
| WeaverKEM-256 | 1312 | 2912 | 1408 | 32 |
| WeaverKEM-512 | 2592 | 5728 | 2944 | 64 |
