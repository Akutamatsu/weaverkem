from proba_util import *
from math import ceil, log2


# def uniform_distr(a, b):
#     D = {}
#     len = b - a + 1
#     for i in range(a, b+1):
#         D[i] = 1 / len
#     return D

def uniform_distr(a, b):
    D = {}
    len = b - a + 1
    for i in range(a, b):
        D[i] = 1 / (len - 1)
    return D

def get_distr(meth, param = 0):
    if meth == "cbd":
        D = build_centered_binomial_law(param) # eta
    elif meth == "gaussian":
        D = approx_discrete_gaussian(param) # sd
    elif meth == "uniform": # half-half; const weight
        D = uniform_distr(-param, param)
    elif meth == "defternary":
        D = build_ternary_law(param) # param is p for prob of non-zero
    elif meth == "ternary": # half-half; const weight
        D = param
    elif meth == "define": 
        D = param # specify the distr itself
    elif meth == "na": # no such term
        D = None
    
    return D

class DistributionSet:
    # Ds, Dr, De, Dep, Depp
    def __init__(self, meths, params, q, du=None, dv=None, dt=None):
        self.q = q
        self.logq = ceil(log2(q))
        if du is None:
            self.du = self.logq
        else:
            self.du = du
        if dv is None:
            self.dv = self.logq
        else:
            self.dv = dv
        if dt is None:
            self.dt = self.logq
            self.t = q
        else:
            self.dt = dt
            self.t = 2**dt

        self.Ds = get_distr(meths[0], params[0])

        # 'None' means copy. 'na' means no such term.
        if meths[1] is None:
            self.Dr = self.Ds
        else:
            self.Dr = get_distr(meths[1], params[1])

        if meths[2] is None:
            self.De = self.Ds
        else:
            self.De = get_distr(meths[2], params[2])

        if meths[3] is None:
            self.Dep = self.De
        else:
            self.Dep = get_distr(meths[3], params[3])

        if meths[4] is None:
            self.Depp = self.Dep
        else:
            self.Depp = get_distr(meths[4], params[4])

        self.meths = {}
        self.meths["s"] = meths[0]
        self.meths["r"] = meths[1]
        self.meths["e"] = meths[2]
        self.meths["ep"] = meths[3]
        self.meths["epp"] = meths[4]

def _normalize_meths_params(meths, params):
    """
    将两种可能的输入格式标准化为长度为5的列表。
    支持：
      - meths: dict {'s':..., 'r':..., 'e':..., 'ep':..., 'epp':...} 或 list/tuple 长度5
      - params: dict 类似或 list/tuple 长度5
    """
    keys = ['s', 'r', 'e', 'ep', 'epp']
    if isinstance(meths, dict):
        meths_list = [meths.get(k, None) for k in keys]
    else:
        meths_list = list(meths)
        if len(meths_list) != 5:
            raise ValueError("meths must be dict(keys: s,r,e,ep,epp) or list of length 5")

    if isinstance(params, dict):
        params_list = [params.get(k, 0) for k in keys]
    else:
        params_list = list(params)
        if len(params_list) != 5:
            raise ValueError("params must be dict(keys: s,r,e,ep,epp) or list of length 5")

    return meths_list, params_list

def build_ds_from_spec(q, meths, params, du=None, dv=None, dt=None):
    """
    根据高层 spec 构造 DistributionSet 并进行组合约束校验。

    参数：
      q: 模数
      meths: dict 或 list 指定每个位置的模式 (s, r, e, ep, epp)
      params: dict 或 list 指定对应参数（与 get_distr 接口一致）
      du, dv, dt: 可选模切位数（用于判断 LWR 是否存在）

    校验规则示例（可根据需要调整）：
      - s 和 r 不能为 None/"na"
      - 如果 De/e/ep/epp 为 None 或 "na"，则对应的模切位数必须存在并且 cbits < log2(q)（表示存在 LWR）
      - 其它组合按 DistributionSet 的约定直接传递
    返回：
      DistributionSet 实例（或抛出 ValueError）
    """
    logq = ceil(log2(q))
    meths_list, params_list = _normalize_meths_params(meths, params)

    # 基本合法性
    if meths_list[0] in (None, "na"):
        raise ValueError("Secret distribution 's' must be specified (not None/'na').")
    if meths_list[1] == "na":
        raise ValueError("Mask/secret 'r' distribution must be specified (not 'na').")

    # 对于 e, ep, epp 的校验：若模式为 None/'na'，则必须通过模切 (cbits < log2(q)）来提供 LWR 误差来源
    # 映射索引: e -> 2 (ct), ep -> 3 (cu), epp -> 4 (cv)
    cbits_map = {2: dt, 3: du, 4: dv}
    for idx in (2, 3, 4):
        meth = meths_list[idx]
        cbits = cbits_map.get(idx, None)
        if meth == "na":
            # 如果没有指定误差分布，则必须有 LWR（即 cbits 提供并 cbits < logq）
            if cbits is None:
                raise ValueError(f"Distribution for index {idx} is 'na' -> corresponding cbits (dt/du/dv) must be provided for LWR.")
            if not (cbits < logq):
                raise ValueError(f"cbits for index {idx} must be < log2(q) to represent LWR when distribution is 'na'. Got cbits={cbits}, log2(q)={logq}.")

    # 构造并返回 DistributionSet
    ds = DistributionSet(meths_list, params_list, q, du=du, dv=dv, dt=dt)
    return ds

def cond_combine_modswitch(q, logq, cbits, noise):
    if cbits < logq:
        A = build_mod_switching_error_law(q, 2**cbits)
    else:
        A = None

    if noise is None:
        D = A
    elif A is None:
        D = noise
    else:
        D = law_convolution(noise, A) # add modswitch error to orig distr

    return D

def accumulate_errdistr(ds, n, m):
    '''
    Suppose As and RA and A is m x n.
    -Ds(Dep + cu) + Dr(De + ct) + (Depp + cv)
    := -s*A + r*B + C
    = E1 + E2 + C
    = F
    '''
    A = cond_combine_modswitch(ds.q, ds.logq, ds.du, ds.Dep) # LWE + Rounding c1: (e' + cu)
    if A is None:
        print ("Error: e' and cu both None.")
        return None

    B = cond_combine_modswitch(ds.q, ds.logq, ds.dt, ds.De) # LWE + Rounding pk: (e + ct)
    if B is None:
        print ("Error: e and ct both None.")
        return None

    # This step need to be careful:
    C0 = cond_combine_modswitch(ds.t, ds.dt, ds.dv, ds.Depp) # LWE + Rounding c2: (e'' + cv)
    if C0 is None:
        print ("Error: e'' and cv both None.")
        return None

    # C = C0    
    C = {}    
    for v, p in C0.items():
        C[v*ds.q//ds.t] = p

    if ds.meths["s"] == "na":
        print ("Error: distribution 's' must be set.")
        return None
    elif ds.meths["s"] == "ternary":
        E1 = iter_law_convolution(A, ds.Ds)
    else:
        D1 = law_product(ds.Ds, A)
        E1 = iter_law_convolution(D1, n)
    
    if ds.meths["r"] == "na":
        print ("Error: distribution 'r' must be set.")
        return None
    elif ds.meths["r"] == "ternary":
        E2 = iter_law_convolution(B, ds.Dr)
    else:
        D2 = law_product(ds.Dr, B)
        E2 = iter_law_convolution(D2, m)

    E = law_convolution(E1, E2)
    F = law_convolution(E, C)
    return F

# def failure_common(ds, n, m, thres, unibd):
#     F = accumulate_errdistr(ds, n, m)
#     if F is None:
#         return -1 # error
#     proba = tail_probability(F, thres)
#     return unibd * proba

def failure_common_rep(ds, n, m, thres, unibd, rep=1):
    F = accumulate_errdistr(ds, n, m)
    if F is None or rep < 1:
        return -1 # error
    if rep != 1:
        E = abs_distribution(F)
        E = iter_law_convolution(E, rep) # rep-dim convolution for D2/D4 (rep=2/4 resp.)
        proba = tail_probability(E, rep * thres)
    else:
        proba = tail_probability(F, thres)
    return unibd * proba

# Usage example (注：用作文档/测试）
if __name__ == "__main__":

    print ("Test...")
    # '''LAC: Not Supported now'''
    # m = n = 512
    # w = 1
    # q=251
    # hws = 256
    # hwr = 256
    # spec_meths = {'s':'ternary', 'r':'ternary', 'e':'ternary', 'ep':'ternary', 'epp':'ternary'} # all ternary??
    # spec_params = [hws, hwr, 256, 256, 256]
    # ds = build_ds_from_spec(q, meths=spec_meths, params=spec_params)

    # thres = q // 2 ** (w+1)
    # f = failure_common(ds, n, m, thres, unibd = 1)
    # print ("failure: %.1f = 2^%.1f"%(f, log(f + 2.**(-300))/log(2)))

