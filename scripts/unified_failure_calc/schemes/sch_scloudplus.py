from schemes.template.scheme_template import SchemeTemplate

# 模板：Scloudplus
ScloudplusTemplate = SchemeTemplate(
    name="ScloudplusTemplate",
    spec_meths={'s':'ternary', 'r':'ternary', 'e':'cbd', 'ep':'cbd', 'epp':'cbd'},
    thres_fn=lambda q,w: q / (2 ** (w + 1)),
    default_unibd=1,
    default_w=1
)

# Not considering BW32 correction:
# Scloudplus128 failure: 7.642e-15 = 2^-46.89
# Scloudplus192 failure: 1.127e-20 = 2^-66.27
# Scloudplus256 failure: 2.493e-24 = 2^-78.41

# [hws, hwr, eta1, eta2, eta2]
Scloudplus128 = ScloudplusTemplate.instantiate(
    inst_name="Scloudplus128",
    spec_params=[300, 300, 7, 7, 7],
    default_m=600,
    default_n=600,
    default_q=2**12,
    default_du=9,
    default_dv=7,
    default_unibd=8*8,
    default_w=2
)

Scloudplus192 = ScloudplusTemplate.instantiate(
    inst_name="Scloudplus192",
    spec_params=[448, 464, 2, 1, 1], # n/2, m/2
    default_m=928,
    default_n=896,
    default_q=2**12,
    default_du=12,
    default_dv=10,
    default_unibd=8*8,
    default_w=3
)

Scloudplus256 = ScloudplusTemplate.instantiate(
    inst_name="Scloudplus256",
    spec_params=[560, 568, 3, 2, 2],
    default_m=1136,
    default_n=1120,
    default_q=2**12,
    default_du=10,
    default_dv=7,
    default_unibd=12*11,
    default_w=2
)