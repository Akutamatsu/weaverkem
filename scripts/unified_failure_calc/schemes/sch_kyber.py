from schemes.template.scheme_template import SchemeTemplate

# 模板：Kyber 的方法类型（只绑定 spec_meths）
KyberParams = SchemeTemplate(
    name="KyberParams",
    spec_meths={'s':'cbd', 'r':None, 'e':None, 'ep':'cbd', 'epp':None},
    thres_fn=lambda q, w: q / 4,   # 示例：Kyber 常用阈值 q/4（可覆盖）
    default_unibd=1,
    default_NN=256,
    default_w=1
)

# [eta1, 0, 0, eta2, 0]:
Kyber512 = KyberParams.instantiate(
    inst_name="Kyber512",
    spec_params=[3, 0, 0, 2, 0],
    default_NN=256,
    default_k=2,
    default_q=3329,
    default_du=10,
    default_dv=4,
    default_unibd=256
)

Kyber768 = KyberParams.instantiate(
    inst_name="Kyber768",
    spec_params=[2, 0, 0, 2, 0],
    default_NN=256,
    default_k=3,
    default_q=3329,
    default_du=10,
    default_dv=4,
    default_unibd=256
)

Kyber1024 = KyberParams.instantiate(
    inst_name="Kyber1024",
    spec_params=[2, 0, 0, 2, 0],
    default_NN=256,
    default_k=4,
    default_q=3329,
    default_du=11,
    default_dv=5,
    default_unibd=256
)