from schemes.template.scheme_template import SchemeTemplate

# 模板：Saber
SaberTemplate = SchemeTemplate(
    name="SaberTemplate",
    spec_meths={'s':'cbd', 'r':None, 'e':'na', 'ep':'na', 'epp':'na'},
    thres_fn=lambda q,w: q / (2 ** (w + 1)),
    default_unibd=1,
    default_NN=256,
    default_w=1
)

# [eta, 0, 0, 0, 0]
LightSaber = SaberTemplate.instantiate(
    inst_name="LightSaber",
    spec_params=[5, 0, 0, 0, 0],
    default_NN=256,
    default_k=2,
    default_q=2**13,
    default_du=10,
    default_dv=3,
    default_dt=10,
    default_unibd=256
)

Saber = SaberTemplate.instantiate(
    inst_name="Saber",
    spec_params=[4, 0, 0, 0, 0],
    default_NN=256,
    default_k=3,
    default_q=2**13,
    default_du=10,
    default_dv=4,
    default_dt=10,
    default_unibd=256
)

FireSaber = SaberTemplate.instantiate(
    inst_name="FireSaber",
    spec_params=[3, 0, 0, 0, 0],
    default_NN=256,
    default_k=4,
    default_q=2**13,
    default_du=10,
    default_dv=6,
    default_dt=10,
    default_unibd=256
)