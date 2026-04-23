from schemes.template.scheme_template import SchemeTemplate

# 模板：SMAUG
SMAUGTemplate = SchemeTemplate(
    name="SMAUGTemplate",
    spec_meths={'s':'ternary', 'r':'ternary', 'e':'gaussian', 'ep':'na', 'epp':'na'},
    thres_fn=lambda q,w: q / (2 ** (w + 1)),
    default_unibd=1,
    default_NN=256,
    default_w=1
)

# [hs, hr, e_sd, 0, 0]
SMAUGT128 = SMAUGTemplate.instantiate(
    inst_name="SMAUGT128",
    spec_params=[140, 132, 1.06, 0, 0],
    # spec_params=[256, 256, 1.06, 0, 0],
    default_NN=256,
    default_k=2,
    default_q=2**10,
    default_du=8,
    default_dv=5,
    default_unibd=256
)

SMAUGT192 = SMAUGTemplate.instantiate(
    inst_name="SMAUGT192",
    spec_params=[198, 151, 1.45, 0, 0],
    # spec_params=[256, 256, 1.06, 0, 0],
    default_NN=256,
    default_k=3,
    default_q=2**11,
    default_du=8,
    default_dv=8,
    default_unibd=256
)

SMAUGT256 = SMAUGTemplate.instantiate(
    inst_name="SMAUGT256",
    spec_params=[176, 160, 1.06, 0, 0],
    # spec_params=[640, 640, 1.06, 0, 0],
    default_NN=256,
    default_k=5,
    default_q=2**11,
    default_du=8,
    default_dv=6,
    default_unibd=256
)