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

def instantiate_weaver_inv(template, config):
    """
    统一创建 Weaver-Inv 参数实例。

    设计目标：
    - 所有核心参数都集中放在一个配置表里；
    - 用户改 dt 时，随机 lifting 误差分布和公钥尺寸会自动同步；
    - 不再需要手动在多个实例块之间同步 eta/n/k/q/dt/du/dv/codebits 等字段。

    配置表字段说明：
    - name:
        参数集名称，会暴露给 estimate_failure.py 作为显示名。
    - eta1, eta2:
        分别对应 s 和 r 的 CBD 参数。
    - nn:
        环维数 n。
    - k:
        module rank。
    - q:
        模数。
    - dt, du, dv:
        分别对应公钥、u、c2 的压缩位数。
    - bch_n, bch_k, bch_t:
        当前这一层所采用的 BCH 三元组。
        其中：
        - bch_n: BCH 码长；
        - bch_k: BCH 信息位数；
        - bch_t: BCH 纠错能力。
    - encoded_bits:
        当前这一层在 Weaver 编码中真正占用的“预 ECC 判决位数”。
        这是 failure estimator 中用于 binomial.sf(t, encoded_bits, p) 的长度。
        encoded_bits = payload_bits + (bch_n - bch_k)，其中 bch_n - bch_k 就是 BCH 的冗余位数。
    - payload_bits:
        当前这一层真正承载的会话密钥比特数。
        自动容量检查会用它来判断整组参数是否真的能装下目标密钥长度。
    - group:
        参数组名。属于同一方案的高位层 / 低位层应共用同一个 group。
    - target_key_bits:
        该 group 需要承载的总会话密钥比特数，例如 128 / 256 / 512。
    """
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
        # 对 Weaver-Inv 而言，公钥误差已经由 build_randinv_error_law(q, dt) 完整建模；
        # 模板中的 default_skip_pk_rounding=True 会保证失败率分析时不再重复叠加
        # 一层 canonical public-key rounding。
        default_dt=config["dt"],
        default_du=config["du"],
        default_dv=config["dv"],
        default_unibd=1,
        # default_codebits=config["encoded_bits"],
        default_codebits= config["payload_bits"] + (config["bch_n"] - config["bch_k"]), # BCH 冗余位数
        default_errtolerance=config["bch_t"],
    )
    # 下面这些属性不参与 compute_failure()，但会被 estimate_failure.py
    # 和参数合法性检查逻辑使用。
    scheme.default_bch_n = config["bch_n"]
    scheme.default_bch_k = config["bch_k"]
    scheme.default_bch_t = config["bch_t"]
    # scheme.default_encoded_bits = config["encoded_bits"]
    scheme.default_payload_bits = config["payload_bits"]
    scheme.capacity_group = config["group"]
    scheme.target_key_bits = config["target_key_bits"]
    return scheme

def validate_weaver_inv_config(config):
    """
    对单个配置做局部合法性检查。

    说明：
    - 这里只检查“单层内部是否自洽”；
    - 跨层的“总承载位数是否够装目标密钥长度”会在后续 group 校验里完成。

    返回：
    - issues: list[str]
      若为空，表示这一层单独看是自洽的。
    """
    issues = []

    if config["payload_bits"] > config["bch_k"]:
        issues.append(
            f"payload_bits={config['payload_bits']} exceeds BCH information bits bch_k={config['bch_k']}"
        )

    if config["bch_k"] > config["bch_n"]:
        issues.append(f"bch_k={config['bch_k']} exceeds bch_n={config['bch_n']}")

    check_encoded_bits = config["payload_bits"] + (config["bch_n"] - config["bch_k"])

    if check_encoded_bits > config["bch_n"]:
        issues.append(
            f"encoded_bits={check_encoded_bits} exceeds BCH length bch_n={config['bch_n']}"
        )

    if config["template"] == "L1":
        if check_encoded_bits > config["nn"]:
            issues.append(
                f"L1 encoded_bits={check_encoded_bits} exceeds nn={config['nn']}"
            )
    elif config["template"] == "L2":
        max_l2_slots = config["nn"] // 4
        if check_encoded_bits > max_l2_slots:
            issues.append(
                f"L2 encoded_bits={check_encoded_bits} exceeds nn/4={max_l2_slots}"
            )
    else:
        issues.append(f"unknown template '{config['template']}'")

    return issues

def annotate_capacity_checks(schemes, configs):
    """
    对整张参数表做分组容量检查，并把结果回写到每个 SchemeInstance 上。

    检查分两层：
    1. 单层检查：
       - payload_bits <= bch_k
       - encoded_bits <= bch_n
       - L1 encoded_bits <= nn
       - L2 encoded_bits <= nn/4
    2. 跨层 group 检查：
       - sum(payload_bits of same group) >= target_key_bits

    这样就能避免出现“DFR 看起来很好，但其实 BCH 根本装不下目标比特数”的情况。
    """
    issues_by_name = {config["name"]: validate_weaver_inv_config(config) for config in configs}

    group_payload = defaultdict(int)
    group_target = {}
    group_members = defaultdict(list)
    for config in configs:
        group = config["group"]
        group_payload[group] += config["payload_bits"]
        group_target[group] = config["target_key_bits"]
        group_members[group].append(config["name"])

    group_messages = {}
    for group, payload in group_payload.items():
        target = group_target[group]
        if payload >= target:
            group_messages[group] = (
                f"[capacity ok] group '{group}' carries {payload} bits >= target {target} bits"
            )
        else:
            group_messages[group] = (
                f"[capacity shortfall] group '{group}' carries {payload} bits < target {target} bits"
            )

    for config in configs:
        scheme = schemes[config["name"]]
        group_msg = group_messages[config["group"]]
        issues = list(issues_by_name[config["name"]])
        if group_payload[config["group"]] < group_target[config["group"]]:
            issues.append(group_msg)

        scheme.validation_issues = issues
        scheme.capacity_summary = group_msg
        scheme.capacity_ok = len(issues) == 0

# 这里的模板仍然沿用旧版 failure estimator 的接口：
# - s/r 表示秘密和临时随机量，均为 CBD；
# - e 表示“公钥项的额外误差分布”；
# - ep / epp 仍然交给原有的 rounding-law 逻辑处理。
#
# 对 Weaver-Inv 来说，公钥项误差已经由 build_randinv_error_law(q, dt) 完整建模，
# 因此失败率分析时不能再把 dt 对应的 canonical rounding 误差重复叠加一遍。
# 现在这件事由 default_skip_pk_rounding=True 统一处理：
# - 用户看到的 default_dt 就是论文里的真实公钥压缩位数 dt；
# - failure 建模时不会重复计入公钥 rounding；
# - calc_size() 也直接使用同一个 dt 计算公钥字节数。
#
# 另外，NGCC_KEM (9).pdf 中把消息嵌入位置改成了“压缩域”：
#   c2 = Encode_dv((Compress_q(v0, dv) + w) mod 2^dv)
# 因此这里统一把 default_c2_mode 设为 "compressed_embed"。


WeaverTemplateL1 = SchemeTemplate(
    name="WeaverTemplateL1",
    spec_meths={"s": "cbd", "r": "cbd", "e": "define", "ep": "na", "epp": "na"},
    thres_fn=lambda q, w: q // (2 ** (w + 1)),
    default_unibd=1,
    default_NN=256,
    default_w=1,
    default_rep=1, # no repetition code.
    default_c2_mode="compressed_embed",
    default_skip_pk_rounding=True,
)


WeaverTemplateL2 = SchemeTemplate(
    name="WeaverTemplateL2",
    spec_meths={"s": "cbd", "r": "cbd", "e": "define", "ep": "na", "epp": "na"},
    thres_fn=lambda q, w: q // (2 ** (w + 1)),
    default_unibd=1,
    default_NN=256,
    default_w=2,
    default_rep=4, # for D4, rep = 4.
    default_c2_mode="compressed_embed",
    default_skip_pk_rounding=True,
)


# Weaver-Inv 的所有核心参数统一维护在这张表里。
# 以后如果你要改方案参数，优先改这里即可。
#
# 使用约定：
# - `template`:
#     "L1" 表示高位 BCH 那一层；
#     "L2" 表示低位 D4 + BCH 那一层。
# - `bch_n, bch_k, bch_t`:
#     真实 BCH 三元组。
# - `encoded_bits`:
#     当前这一层真正进入错误率估计器的长度。
#     对 L1 往往等于 bch_n；
#     对 L2 往往对应论文里的 l3，因此可能小于 bch_n。
# - `payload_bits`:
#     当前这一层真正承载的会话密钥比特数。
# - `group, target_key_bits`:
#     用于自动检查整组参数是否真的能装下目标会话密钥长度。

# WEAVER_INV_CONFIGS = [
#     {
#         "name": "Weaver128L1",
#         "template": "L1",
#         "group": "Weaver128",
#         "target_key_bits": 128,
#         "eta1": 2,
#         "eta2": 2,
#         "nn": 128,
#         "k": 5,
#         "q": 3329,
#         "dt": 9,
#         "du": 9,
#         "dv": 6,
#         "bch_n": 127,
#         "bch_k": 113,
#         "bch_t": 2,
#         "payload_bits": 112,
#     },
#     {
#         "name": "Weaver128L2",
#         "template": "L2",
#         "group": "Weaver128",
#         "target_key_bits": 128,
#         "eta1": 2,
#         "eta2": 2,
#         "nn": 128,
#         "k": 5,
#         "q": 3329,
#         "dt": 9,
#         "du": 9,
#         "dv": 6,
#         "bch_n": 31,
#         "bch_k": 21,
#         "bch_t": 2,
#         "payload_bits": 128-112,
#     },
#     # {
#     #     "name": "Weaver128L1",
#     #     "template": "L1",
#     #     "group": "Weaver128",
#     #     "target_key_bits": 128,
#     #     "eta1": 2,
#     #     "eta2": 2,
#     #     "nn": 256,
#     #     "k": 3,
#     #     "q": 3329,
#     #     "dt": 9,
#     #     "du": 8,
#     #     "dv": 6,
#     #     "bch_n": 255,
#     #     "bch_k": 215,
#     #     "bch_t": 5,
#     #     # "bch_k": 223,
#     #     # "bch_t": 4,
#     #     # "encoded_bits": autocalc
#     #     "payload_bits": 128,
#     # },
#     # {
#     #     "name": "Weaver256L1Ex",
#     #     "template": "L1",
#     #     "group": "Weaver256Ex",
#     #     "target_key_bits": 256,
#     #     "eta1": 3,
#     #     "eta2": 3,
#     #     "nn": 512,
#     #     "k": 2,
#     #     "q": 3329,
#     #     "dt": 9,
#     #     "du": 9,
#     #     "dv": 4,
#     #     "bch_n": 511,
#     #     "bch_k": 421,
#     #     "bch_t": 10,
#     #     "payload_bits": 256,
#     # },
#     # {
#     #     "name": "Weaver256L1",
#     #     "template": "L1",
#     #     "group": "Weaver256",
#     #     "target_key_bits": 256,
#     #     "eta1": 3,
#     #     "eta2": 2,
#     #     "nn": 256,
#     #     "k": 5,
#     #     "q": 3329,
#     #     "dt": 10,
#     #     "du": 9,
#     #     "dv": 6,
#     #     "bch_n": 255,
#     #     "bch_k": 223,
#     #     "bch_t": 4,
#     #     # "encoded_bits": autocalc
#     #     "payload_bits": 223,
#     # },
#     # {
#     #     "name": "Weaver256L2",
#     #     "template": "L2",
#     #     "group": "Weaver256",
#     #     "target_key_bits": 256,
#     #     "eta1": 3,
#     #     "eta2": 2,
#     #     "nn": 256,
#     #     "k": 5,
#     #     "q": 3329,
#     #     "dt": 10,
#     #     "du": 9,
#     #     "dv": 6,
#     #     "bch_n": 63,
#     #     "bch_k": 39,
#     #     "bch_t": 4,
#     #     # "encoded_bits": autocalc
#     #     "payload_bits": 256-223,
#     # },
#     {
#         "name": "Weaver256L1",
#         "template": "L1",
#         "group": "Weaver256",
#         "target_key_bits": 256,
#         "eta1": 4,
#         "eta2": 4,
#         "nn": 256,
#         "k": 5,
#         "q": 7681,
#         "dt": 10,
#         "du": 9,
#         "dv": 8,
#         "bch_n": 255,
#         "bch_k": 223,
#         "bch_t": 4,
#         # "encoded_bits": autocalc
#         "payload_bits": 223,
#     },
#     {
#         "name": "Weaver256L2",
#         "template": "L2",
#         "group": "Weaver256",
#         "target_key_bits": 256,
#         "eta1": 4,
#         "eta2": 4,
#         "nn": 256,
#         "k": 5,
#         "q": 7681,
#         "dt": 10,
#         "du": 9,
#         "dv": 8,
#         "bch_n": 63,
#         "bch_k": 39,
#         "bch_t": 4,
#         # "encoded_bits": autocalc
#         "payload_bits": 256-223,
#     },
#     {
#         "name": "Weaver512L1",
#         "template": "L1",
#         "group": "Weaver512",
#         "target_key_bits": 512,
#         "eta1": 3,
#         "eta2": 3,
#         "nn": 512,
#         "k": 4,
#         "q": 3329,
#         "dt": 10,
#         "du": 10,
#         "dv": 4,
#         "bch_n": 511,
#         # "bch_k": 448,
#         # "bch_t": 7,
#         # "payload_bits": 448,
#         "bch_k": 457,
#         "bch_t": 6,
#         "payload_bits": 456,
#         # "bch_k": 466,
#         # "bch_t": 5,
#         # "payload_bits": 464,
#         # "encoded_bits": autocalc
#     },
#     {
#         "name": "Weaver512L2",
#         "template": "L2",
#         "group": "Weaver512",
#         "target_key_bits": 512,
#         "eta1": 3,
#         "eta2": 3,
#         "nn": 512,
#         "k": 4,
#         "q": 3329,
#         "dt": 10,
#         "du": 10,
#         "dv": 4,
#         "bch_n": 127,
#         # "bch_k": 78,
#         # "bch_t": 7,
#         # "payload_bits": 512-448,
#         "bch_k": 85,
#         "bch_t": 6,
#         "payload_bits": 512-456,
#         # "encoded_bits": autocalc
#     },
# ]

WEAVER_INV_CONFIGS = [
    {
        "name": "Weaver128L1",
        "template": "L1",
        "group": "Weaver128",
        "target_key_bits": 128,
        "eta1": 3,
        "eta2": 2,
        "nn": 128,
        "k": 5,
        "q": 3329,
        "dt": 9,
        "du": 9,
        "dv": 6,
        "bch_n": 127,
        "bch_k": 113,
        "bch_t": 2,
        "payload_bits": 112,
    },
    {
        "name": "Weaver128L2",
        "template": "L2",
        "group": "Weaver128",
        "target_key_bits": 128,
        "eta1": 3,
        "eta2": 2,
        "nn": 128,
        "k": 5,
        "q": 3329,
        "dt": 9,
        "du": 9,
        "dv": 6,
        "bch_n": 31,
        "bch_k": 21,
        "bch_t": 2,
        "payload_bits": 128-112,
    },
    # {
    #     "name": "Weaver128L1",
    #     "template": "L1",
    #     "group": "Weaver128",
    #     "target_key_bits": 128,
    #     "eta1": 6,
    #     "eta2": 4,
    #     "nn": 128,
    #     "k": 5,
    #     "q": 7681,
    #     "dt": 10,
    #     "du": 9,
    #     "dv": 6,
    #     "bch_n": 127,
    #     "bch_k": 113,
    #     "bch_t": 2,
    #     "payload_bits": 112,
    # },
    # {
    #     "name": "Weaver128L2",
    #     "template": "L2",
    #     "group": "Weaver128",
    #     "target_key_bits": 128,
    #     "eta1": 6,
    #     "eta2": 4,
    #     "nn": 128,
    #     "k": 5,
    #     "q": 7681,
    #     "dt": 10,
    #     "du": 9,
    #     "dv": 6,
    #     "bch_n": 31,
    #     "bch_k": 21,
    #     "bch_t": 2,
    #     "payload_bits": 128-112,
    # },
    {
        "name": "Weaver256L1",
        "template": "L1",
        "group": "Weaver256",
        "target_key_bits": 256,
        "eta1": 7,
        "eta2": 7,
        "nn": 256,
        "k": 4,
        "q": 7681,
        "dt": 10,
        "du": 10,
        "dv": 8,
        "bch_n": 255,
        "bch_k": 223,
        "bch_t": 4,
        # "encoded_bits": autocalc
        "payload_bits": 223,
    },
    {
        "name": "Weaver256L2",
        "template": "L2",
        "group": "Weaver256",
        "target_key_bits": 256,
        "eta1": 7,
        "eta2": 7,
        "nn": 256,
        "k": 4,
        "q": 7681,
        "dt": 10,
        "du": 10,
        "dv": 8,
        "bch_n": 63,
        "bch_k": 39,
        "bch_t": 4,
        # "encoded_bits": autocalc
        "payload_bits": 256-223,
    },
    {
        "name": "Weaver512L1",
        "template": "L1",
        "group": "Weaver512",
        "target_key_bits": 512,
        "eta1": 9,
        "eta2": 9,
        "nn": 512,
        "k": 4,
        "q": 7681,
        "dt": 11,
        "du": 11,
        "dv": 9,
        "bch_n": 511,
        "bch_k": 448,
        "bch_t": 7,
        "payload_bits": 448,
        # "bch_k": 457,
        # "bch_t": 6,
        # "payload_bits": 456,
        # "bch_k": 466,
        # "bch_t": 5,
        # "payload_bits": 464,
        # "encoded_bits": autocalc
    },
    {
        "name": "Weaver512L2",
        "template": "L2",
        "group": "Weaver512",
        "target_key_bits": 512,
        "eta1": 9,
        "eta2": 9,
        "nn": 512,
        "k": 4,
        "q": 7681,
        "dt": 11,
        "du": 11,
        "dv": 9,
        "bch_n": 127,
        "bch_k": 78,
        "bch_t": 7,
        "payload_bits": 512-448,
        # "bch_k": 85,
        # "bch_t": 6,
        # "payload_bits": 512-456,
        # "encoded_bits": autocalc
    },
]


def build_weaver_inv_schemes():
    """
    根据上面的单表配置批量生成所有 Weaver-Inv 参数实例。

    返回：
    - 一个 dict，key 是参数集名称，value 是对应的 SchemeInstance。
    """
    template_map = {
        "L1": WeaverTemplateL1,
        "L2": WeaverTemplateL2,
    }

    schemes = {}
    for config in WEAVER_INV_CONFIGS:
        template_key = config["template"]
        if template_key not in template_map:
            raise KeyError(f"unknown template '{template_key}' in WEAVER_INV_CONFIGS")
        schemes[config["name"]] = instantiate_weaver_inv(template_map[template_key], config)
    annotate_capacity_checks(schemes, WEAVER_INV_CONFIGS)
    return schemes


_WEAVER_INV_SCHEMES = build_weaver_inv_schemes()

Weaver128L1 = _WEAVER_INV_SCHEMES["Weaver128L1"]
Weaver128L2 = _WEAVER_INV_SCHEMES["Weaver128L2"]
# Weaver256L1Ex = _WEAVER_INV_SCHEMES["Weaver256L1Ex"]
Weaver256L2 = _WEAVER_INV_SCHEMES["Weaver256L2"]
Weaver256L1 = _WEAVER_INV_SCHEMES["Weaver256L1"]
Weaver512L2 = _WEAVER_INV_SCHEMES["Weaver512L2"]
Weaver512L1 = _WEAVER_INV_SCHEMES["Weaver512L1"]
