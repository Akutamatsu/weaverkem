# ConstantTimeRef 目录说明（完整快照）

本目录为 **`weaverkem/ref_ct_timing_fix`** 在 **2026-05-15** 的**完整拷贝**：包含 **`src/`**、**`correct/`**、**`speed_test/`**、**`timecop/`**、**`sample_data/`** 等，**可独立按各子目录 `Makefile` 或既有脚本编译运行**（与拷贝源目录行为一致）。

## 与仓库其它路径的关系

- **开发主副本**仍在：`weaverkem/ref_ct_timing_fix/`。  
- 本目录用于归档「当前可编译运行的常数时间 BCH 等改动」整树；若两处需长期一致，请以 **`ref_ct_timing_fix`** 为准再同步覆盖本目录。

## 当日实验结论（摘要）

详见此前归档说明（亦可对照 **`timecop/timecop_valgrind_WEAVER-*.log`**）：

- **正确性**：`correct/` 下 `compare_ref_impl` 与 `ref` 侧 `kat_ref_*` 对齐；`correct_test` 大规模随机无失败。  
- **性能**：`speed_test/test_speed_sync.c` 固定 derand 下，`decaps` 相对 `ref` 约 **+10%～+12%**（依档位），`keypair/encaps` 接近。  
- **TIMECOP**：WSL + Valgrind Memcheck，`poison(sk)` 后 `dec`；BCH 高层路径不再出现在 512/1024 的 Memcheck 栈顶；剩余报告主要来自 **`poly_invq` / `indcpa` 的 `rej_uniform`**（与 `ref` 同构分支）。

## 快速编译示例（Linux / WSL + gcc）

```bash
cd correct && make clean && make
cd ../speed_test && make clean && make
cd ../timecop && bash ./run_timecop.sh
```

Windows 下可用 **MSYS2 UCRT64 `gcc`**，对 **`correct/Makefile`** 将 **`CC=`** 改为本机 `gcc` 路径后执行 **`mingw32-make`**（或手写与此前会话相同的 `gcc` 命令行）。
