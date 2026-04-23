from schemes.template.scheme_template import SchemeTemplate

# ----------------------------
# 预定义示例：先构建模板，然后用模板生成具体实例
# ----------------------------
# 模板：Frodo 的方法类型（只绑定 spec_meths）
FrodoParams = SchemeTemplate(
    name="FrodoParams",
    spec_meths={'s':'define', 'r':None, 'e':None, 'ep':None, 'epp':None}, # None means the same as 's'.
    thres_fn=lambda q, w: q / 2**(w + 1),  # 示例：Frodo 常用阈值 q/2^(w + 1)（可覆盖）
    default_unibd=1,
    default_k=0,
    default_w=2
)

# [eta1, 0, 0, eta2, 0]:
def dist_frodo(n):
    if n == 640:
        T = [9288, 8720, 7216, 5264, 3384, 1918, 958, 422, 164, 56, 17, 4, 1]
    elif n == 976:
        T = [11278, 10277, 7774, 4882, 2545, 1101, 396, 118, 29, 6, 1]
    elif n == 1344:
        T = [18286, 14320, 6876, 2023, 364, 40, 2]
    else:
        raise ValueError("Unsupported n")
    D = {}
    for (i, j) in enumerate(T):
        D[i] = 2**-16 * j
        D[-i] = 2**-16 * j
    return D

Frodo640 = FrodoParams.instantiate(
    inst_name="Frodo640",
    spec_params=[dist_frodo(640), 0, 0, 0, 0],
    default_m=640,
    default_n=640,
    default_q=2**15,
    default_unibd=8*8,
    default_w=2
)