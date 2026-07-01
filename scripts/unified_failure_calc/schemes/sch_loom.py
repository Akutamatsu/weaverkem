from schemes.template.scheme_template import SchemeTemplate
from functools import lru_cache
from math import log2, ceil
from collections import defaultdict
from proba_util import build_uniform_law, law_convolution, mod_centered

def build_range_dict(input_dict, q):
    """
    将字典 {x: y} 转换为 {y: [min(y-x), max(y-x)]}
    """
    range_dict = defaultdict(lambda: [float('inf'), -float('inf')])

    for x, y in input_dict.items():
        diff = mod_centered(y - x, q)
        current = range_dict[y]
        if diff < current[0]:
            current[0] = diff
        if diff > current[1]:
            current[1] = diff
    # 将 defaultdict 转换为普通 dict（可选）
    return dict(range_dict)

@lru_cache(maxsize=None)
def build_randinv_decompress_law(q, du):
    if ceil(log2(q)) <= du:
        raise ValueError("du must be smaller than log2(q)")
    power_du = 2 ** du
    bucket_map = {}
    for x in range(q):
        y = (x * power_du + q//2) // q % power_du
        x_prime = (y * q + power_du//2) // power_du
        e = mod_centered(x_prime - x, q)
        # if x > 3320:
        #     print(f"x: {x}, y: {y}, x': {x_prime}, error: {e}")
        bucket_map[x] = x_prime
    E = {}
    # for key, val in bucket_map.items():
    #     if key > 3320:
    #         print(f"x: {key}, x': {val}, error: {mod_centered(val - key, q)}")
    map_table = build_range_dict(bucket_map, q)
    # for idx, (x_prime, (a, b)) in enumerate(map_table.items()):
    #     if idx == len(map_table) - 1:
    #         print(f"x': {x_prime}, range: [{a}, {b}]")
    for x, x_prime in bucket_map.items():
        a = map_table[x_prime][0]
        b = map_table[x_prime][1]
        # len = b - a + 1
        tmp_dist = build_uniform_law(a, b) # [a, b]
        e = mod_centered(x_prime - x, q)
        det_dist = {e: 1./q}
        add_dist = law_convolution(tmp_dist, det_dist)
        for key, val in add_dist.items():
            E[key] = E.get(key, 0) + val
    return E

def instantiate_loom_inv(template, config):
    scheme = template.instantiate(
        inst_name=config["name"],
        spec_params=[
            config["eta1"],
            config["eta2"],
            build_randinv_decompress_law(config["q"], config["dt"]),
            0,
            0,
        ],
        default_NN=config["nn"],
        default_k=config["k"],
        default_q=config["q"],
        # 这里 default_dt 直接表示论文里的真实公钥压缩位数 dt。
        # 对 Loom-Inv 而言，公钥误差已经由 build_randinv_error_law(q, dt) 完整建模；
        # 模板中的 default_skip_pk_rounding=True 会保证失败率分析时不再重复叠加
        # 一层 canonical public-key rounding。
        default_dt=config["dt"],
        default_du=config["du"],
        default_dv=config["dv"],
        default_unibd=config["payload_bits"], # 表示只要错误分布里有一个位置出错就算失败。
        default_rep = config.get("default_rep", 1), # 默认为 1，即不使用重复码；若 config 中指定了 default_rep 则使用它。
        default_pkseedlen = config["default_pkseedlen"],
        default_cttaglen = config["default_cttaglen"],
    )
    return scheme

LoomTemplateL1 = SchemeTemplate(
    name="LoomTemplateL1",
    spec_meths={"s": "cbd", "r": "cbd", "e": "define", "ep": "na", "epp": "na"},
    thres_fn=lambda q, w: q // (2 ** (w + 1)),
    default_unibd=1,
    default_NN=256,
    default_w=1,
    default_rep=1, # no repetition code.
    default_c2_mode="compressed_embed",
    default_skip_pk_rounding=True,
)

# LOOM_CONFIGS = [
#     {
#         "name": "Loom128L1",
#         "template": "L1",
#         "eta1": 5,
#         "eta2": 5,
#         "nn": 256,
#         "k": 2,
#         "q": 3329,
#         "dt": 9,
#         "du": 8,
#         "dv": 5,
#         "payload_bits": 128,
#         # "default_rep": 2, # Loom128L1 使用重复码，默认重复次数为 2。
#     },
#     # {
#     #     "name": "Loom256L1",
#     #     "template": "L1",
#     #     "eta1": 3,
#     #     "eta2": 2,
#     #     "nn": 256,
#     #     "k": 4,
#     #     "q": 3329,
#     #     "dt": 9,
#     #     "du": 8,
#     #     "dv": 5,
#     #     "payload_bits": 256,
#     # },
#     {
#         "name": "Loom256L1",
#         "template": "L1",
#         "eta1": 1,
#         "eta2": 1,
#         "nn": 256,
#         "k": 4,
#         "q": 769,
#         "dt": 8,
#         "du": 8,
#         "dv": 5,
#         "payload_bits": 256,
#     },
#     {
#         "name": "Loom512L1",
#         "template": "L1",
#         "eta1": 1,
#         "eta2": 1,
#         "nn": 512,
#         "k": 4,
#         "q": 3329,
#         "dt": 9,
#         "du": 8,
#         "dv": 4,
#         "payload_bits": 512,
#     },
# ]
LOOM_CONFIGS = [
    {
        "name": "Loom128L1",
        "template": "L1",
        "eta1": 3,
        "eta2": 2,
        "nn": 128,
        "k": 5,
        "q": 3329,
        "dt": 9,
        "du": 8,
        "dv": 4,
        "payload_bits": 128,
        "default_pkseedlen": 32,
        "default_cttaglen": 32,
    },
    {
        "name": "Loom256L1",
        "template": "L1",
        "eta1": 8,
        "eta2": 4,
        "nn": 256,
        "k": 4,
        "q": 7681,
        "dt": 10,
        "du": 9,
        "dv": 4,
        "payload_bits": 256,
        "default_pkseedlen": 32,
        "default_cttaglen": 64,
    },
    {
        "name": "Loom512L1",
        "template": "L1",
        "eta1": 10,
        "eta2": 8,
        "nn": 512,
        "k": 4,
        "q": 7681,
        "dt": 11,
        "du": 10,
        "dv": 4,
        "payload_bits": 512,
        "default_pkseedlen": 64,
        "default_cttaglen": 128,
    },
    # {
    #     "name": "Loom512L1",
    #     "template": "L1",
    #     "eta1": 9,
    #     "eta2": 9,
    #     "nn": 512,
    #     "k": 4,
    #     "q": 7681,
    #     "dt": 12,
    #     "du": 11,
    #     "dv": 8,
    #     "payload_bits": 512,
    # },
]

def build_loom_inv_schemes():
    """
    根据上面的单表配置批量生成所有 Loom-Inv 参数实例。

    返回：
    - 一个 dict，key 是参数集名称，value 是对应的 SchemeInstance。
    """
    template_map = {
        "L1": LoomTemplateL1,
        # "L2": LoomTemplateL2,
    }

    schemes = {}
    for config in LOOM_CONFIGS:
        template_key = config["template"]
        if template_key not in template_map:
            raise KeyError(f"unknown template '{template_key}' in LOOM_CONFIGS")
        schemes[config["name"]] = instantiate_loom_inv(template_map[template_key], config)
    return schemes


_LOOM_SCHEMES = build_loom_inv_schemes()

Loom128 = _LOOM_SCHEMES["Loom128L1"]
Loom256 = _LOOM_SCHEMES["Loom256L1"]
Loom512 = _LOOM_SCHEMES["Loom512L1"]
