--- v2.1 ---
1. 修复错误率计算脚本之前的边界错误 (增加 mod_centered)
2. 尝试评估压缩后嵌入方法的误差分布及错误率 
(当 ds.c2_mode == "compressed_embed" 时, 在 failure_common.py 中调用 build_asymmetric_error_law)

--- v2.0 ---
新增： sch_weaver_inv.py

请打开 estimate_failure.py 并运行。
使用方法见同文件开头注释。