from schemes.template.scheme_template import SchemeTemplate

# LAC
LACTemplate = SchemeTemplate(
    name="LACTemplate",
    spec_meths={'s':'ternary', 'r':'ternary', 'e':'defternary', 'ep':None, 'epp':None},
    thres_fn=lambda q,w: q / (2 ** (w + 1)),
    default_unibd=1,
    #default_NN=256,
    default_w=1,
    # default_rep=1, # w/o D2
    default_rep=2, # D2 encoding
)

# [hs, hr, e_sd, 0, 0]
LAC128 = LACTemplate.instantiate(
    inst_name="LAC128",
    spec_params=[256, 0, 256/512, 0, 0],
    default_NN=512,
    default_k=1,
    default_q=251,
    default_unibd=1,
    default_codebits=128+64, # 64 bits for BCH(255,8,191) code
    default_errtolerance=8,
)

LAC192 = LACTemplate.instantiate(
    inst_name="LAC192",
    spec_params=[256, 0, 256/1024, 0, 0],
    default_NN=1024,
    default_k=1,
    default_q=251,
    default_unibd=1,
    default_codebits=256+72, # 72 bits for BCH(511,8,439) code
    default_errtolerance=8,
)

LAC256 = LACTemplate.instantiate(
    inst_name="LAC256",
    spec_params=[384, 0, 384/1024, 0, 0],
    default_NN=1024,
    default_k=1,
    default_q=251,
    default_unibd=1,
    default_codebits=256+171, # 171 bits for BCH(511,20,340) code
    default_errtolerance=20,
)