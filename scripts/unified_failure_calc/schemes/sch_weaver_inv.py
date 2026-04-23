from schemes.template.scheme_template import SchemeTemplate
from math import log2, ceil
from collections import defaultdict
from proba_util import build_uniform_law, law_convolution

def build_range_dict(input_dict):
    """
    将字典 {x: y} 转换为 {y: [min(y-x), max(y-x)]}
    """
    range_dict = defaultdict(lambda: [float('inf'), -float('inf')])

    for x, y in input_dict.items():
        diff = y - x
        current = range_dict[y]
        if diff < current[0]:
            current[0] = diff
        if diff > current[1]:
            current[1] = diff
    # 将 defaultdict 转换为普通 dict（可选）
    return dict(range_dict)

def build_randinv_decompress_law(q, du):
    if ceil(log2(q)) <= du:
        raise ValueError("du must be smaller than log2(q)")
    power_du = 2 ** du
    bucket_map = {}
    for x in range(q):
        y = (x * power_du + q//2) // q 
        x_prime = (y * q + power_du//2) // power_du
        e = x_prime - x
        # if e == 7:
        #     print(f"x: {x}, y: {y}, x': {x_prime}, error: {e}")
        bucket_map[x] = x_prime
    E = {}
    map_table = build_range_dict(bucket_map)
    for x, y in bucket_map.items():
        a = map_table[y][0]
        b = map_table[y][1]
        # len = b - a + 1
        tmp_dist = build_uniform_law(a, b) # [a, b]
        e = y - x
        det_dist = {e: 1./q}
        add_dist = law_convolution(tmp_dist, det_dist)
        for key, val in add_dist.items():
            E[key] = E.get(key, 0) + val
    return E

# 模板：WeaverTemplateL1
WeaverTemplateL1 = SchemeTemplate(
    name="WeaverTemplateL1",
    spec_meths={'s':'cbd', 'r':'cbd', 'e':'define', 'ep':'na', 'epp':'na'},
    thres_fn=lambda q,w: q // (2 ** (w + 1)),
    default_unibd=1,
    default_NN=256,
    default_w=1,
    default_rep=1, # no repetition code.
)

# cmpr8_law = build_randinv_decompress_law(3329, 8)
cmpr9_law = build_randinv_decompress_law(3329, 9)
cmpr10_law = build_randinv_decompress_law(3329, 10)

Weaver128 = WeaverTemplateL1.instantiate(
    inst_name="Weaver128",
    # spec_params=[2, 2, cmpr8_law,0,0], # failure only depends on here, not on k.
    spec_params=[3, 2, cmpr9_law,0,0], # failure only depends on here, not on k.
    default_NN=256,
    default_k=2,
    default_q=3329,
    default_du=8,
    default_dv=4,
    # default_dt=8,
    default_unibd=1,
    # default_codebits=128+24, # 24 bits for BCH (255,3)
    default_codebits=128+40, # 40 bits for BCH (255,5)
    default_errtolerance=5 # correct up to 5 bits error.
)

# 模板：WeaverTemplateL2
WeaverTemplateL2 = SchemeTemplate(
    name="WeaverTemplateL2",
    spec_meths={'s':'cbd', 'r':'cbd', 'e':'define', 'ep':'na', 'epp':'na'},
    thres_fn=lambda q,w: q // (2 ** (w + 1)),
    default_unibd=1,
    default_NN=256,
    default_w=2,
    default_rep=4, # for D4, rep = 4.
    # default_rep=2, # for D4, rep = 4.
)

Weaver256 = WeaverTemplateL2.instantiate(
    inst_name="Weaver256",
    # spec_params=[2, 2, cmpr9_law,0,0],
    spec_params=[3, 2, cmpr10_law,0,0],
    default_NN=256,
    default_k=4,
    default_q=3329,
    default_du=9,
    default_dv=4,
    # default_dt=9,
    default_unibd=1,
    default_codebits=64,
    default_errtolerance=4
    # default_errtolerance=3 # correct up to 3 bits error.
)
Weaver256L1 = WeaverTemplateL1.instantiate(
    inst_name="Weaver256L1",
    # spec_params=[2, 2, cmpr9_law,0,0],
    spec_params=[3, 2, cmpr10_law,0,0],
    default_NN=256,
    default_k=4,
    default_q=3329,
    default_du=9,
    default_dv=4,
    # default_dt=9,
    default_unibd=1,
    default_codebits=231+24,
    default_errtolerance=4 # correct up to 4 bits error.
)

# Weaver512 = WeaverTemplateL2.instantiate(
#     inst_name="Weaver512",
#     spec_params=[2, 2, 0,0,0],
#     default_NN=512,
#     default_k=4,
#     default_q=3329,
#     default_du=10,
#     default_dv=4,
#     default_dt=10,
#     default_unibd=1,
#     default_codebits=64,
#     default_errtolerance=3 # correct up to 3 bits error.
# )
# Weaver512L1 = WeaverTemplateL1.instantiate(
#     inst_name="Weaver512L1",
#     spec_params=[2, 2, 0,0,0],
#     default_NN=512,
#     default_k=4,
#     default_q=3329,
#     default_du=10,
#     default_dv=4,
#     default_dt=10,
#     default_unibd=1,
#     default_codebits=480+27,
#     default_errtolerance=3 # correct up to 3 bits error.
# )
Weaver512 = WeaverTemplateL2.instantiate(
    inst_name="Weaver512",
    # spec_params=[1, 1, cmpr10_law,0,0],
    spec_params=[2, 1, cmpr10_law,0,0],
    default_NN=512,
    default_k=4,
    default_q=3329,
    default_du=9,
    default_dv=6,
    # default_dt=9,
    default_unibd=1,
    default_codebits=40+28, # 28 bits for BCH (127,4)
    default_errtolerance=5 # correct up to 4 bits error.
)
Weaver512L1 = WeaverTemplateL1.instantiate(
    inst_name="Weaver512L1",
    # spec_params=[1, 1, cmpr10_law,0,0],
    spec_params=[2, 1, cmpr10_law,0,0],
    default_NN=512,
    default_k=4,
    default_q=3329,
    default_du=9,
    default_dv=6,
    # default_dt=9,
    default_unibd=1,
    default_codebits=470+36, # 36 bits for BCH (511,4)
    default_errtolerance=5 # correct up to 4 bits error.
)
