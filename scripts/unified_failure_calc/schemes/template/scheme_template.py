from typing import Any, Dict, List, Optional, Callable
from failure_common import build_ds_from_spec, failure_common_rep
from math import ceil, log2

class SchemeTemplate:
    """
    模板对象：绑定 spec_meths（顺序列表或 dict）和默认阈值策略/unibd 等。
    使用 instantiate(...) 创建具体的 SchemeInstance（绑定 spec_params 等）。

    约定：
    - default_k = 0 表示普通 LWE；
    - default_k = 1 表示 RLWE；
    - default_k > 1 表示 MLWE / Module-LWE。

    说明：
    - default_dt / default_du / default_dv 既参与分布构造，也会参与尺寸计算。
    - 对某些特殊方案（例如 Weaver-Inv），失败率分析里我们会直接给出“公钥 lifting
      误差”的完整分布，此时如果再把 default_dt 当作公钥 rounding 位数叠加进去，
      会发生重复计数。
    - 为了避免这个问题，这里额外提供 default_size_dt：它只用于 calc_size()，不参
      与失败率分布的构造。这样就可以同时做到：
      1) 失败率计算不重复计入公钥误差；
      2) 输出尺寸仍然按照论文中的真实压缩位数计算。
    - default_c2_mode 用来描述第二个密文分量 c2 的消息嵌入方式：
      1) "ring_embed": 旧版，先在环上把消息加到 v0 中，再压缩；
      2) "compressed_embed": 新版，先压缩 v0，再在压缩域里把消息加到 c2 上。
    - default_skip_pk_rounding 用来描述公钥项是否还需要额外叠加 canonical
      decompress 误差：
      1) False: 普通方案，按 dt 正常叠加公钥 rounding；
      2) True: 方案已经直接给出公钥 representative-selection / lifting 的完整误差，
         此时不应再额外叠加一层 rounding。
    """
    def __init__(self,
                 name: str,
                 spec_meths: Any,
                 thres_fn: Optional[Callable[[int,int], float]] = None,
                 default_unibd: int = 1,
                 default_NN: Optional[int] = None,
                 default_k: int = 0, 
                 default_w: Optional[int] = None,
                 default_rep: int = 1,
                 default_size_dt: Optional[int] = None,
                 default_c2_mode: str = "ring_embed",
                 default_skip_pk_rounding: bool = False,
                 default_codebits: Optional[int] = None,
                 default_errtolerance: int = 0):
        self.name = name
        self.spec_meths = spec_meths
        self.default_unibd = default_unibd
        self.default_w = default_w
        self.default_NN = default_NN
        self.default_k = default_k
        self.thres_fn = thres_fn if thres_fn is not None else (lambda q, w: q / (2 ** (w + 1)))
        self.default_rep = default_rep
        self.default_size_dt = default_size_dt
        self.default_c2_mode = default_c2_mode
        self.default_skip_pk_rounding = default_skip_pk_rounding
        self.default_codebits = default_codebits
        self.default_errtolerance = default_errtolerance

    def instantiate(self,
                    inst_name: str,
                    spec_params: Optional[List[Any]] = None,
                    default_NN: Optional[int] = None,
                    default_k: Optional[int] = None,
                    default_m: Optional[int] = None,
                    default_n: Optional[int] = None,
                    default_q: Optional[int] = None,
                    default_du: Optional[int] = None,
                    default_dv: Optional[int] = None,
                    default_dt: Optional[int] = None,
                    default_unibd: Optional[int] = None,
                    default_w: Optional[int] = None,
                    default_rep: Optional[int] = None,
                    default_size_dt: Optional[int] = None,
                    default_c2_mode: Optional[str] = None,
                    default_skip_pk_rounding: Optional[bool] = None,
                    default_codebits: Optional[int] = None,
                    default_errtolerance: Optional[int] = None):
        """
        创建并返回 SchemeInstance。实例可覆盖模板的 default_unibd，并可绑定默认 n/m。
        spec_params: 顺序列表 [s_param, r_param, e_param, ep_param, epp_param]
        """
        # 将实例化时传入的 default_k/default_NN 传递给 SchemeInstance（若为 None 则由 SchemeInstance 回退到模板）
        return SchemeInstance(
            template=self,
            name=inst_name,
            spec_params=spec_params,
            default_NN=default_NN,
            default_k=default_k,
            default_n=default_n,
            default_m=default_m,
            default_q=default_q,
            default_du=default_du,
            default_dv=default_dv,
            default_dt=default_dt,
            default_unibd=(default_unibd if default_unibd is not None else self.default_unibd),
            default_w=(default_w if default_w is not None else self.default_w),
            default_rep=default_rep,
            default_size_dt=(default_size_dt if default_size_dt is not None else self.default_size_dt),
            default_c2_mode=(default_c2_mode if default_c2_mode is not None else self.default_c2_mode),
            default_skip_pk_rounding=(
                default_skip_pk_rounding
                if default_skip_pk_rounding is not None
                else self.default_skip_pk_rounding
            ),
            default_codebits=(default_codebits if default_codebits is not None else self.default_codebits),
            default_errtolerance=(default_errtolerance if default_errtolerance is not None else self.default_errtolerance)
        )

class SchemeInstance:
    """
    绑定了模板（spec_meths）和具体参数（spec_params、q、du/dv/dt、n/m、unibd）。
    可直接调用 compute_failure(...) 来计算错误率，若未传入 n/m 则使用实例绑定值。
    """
    def __init__(self,
                 template: SchemeTemplate,
                 name: str,
                 spec_params: Optional[List[Any]] = None,
                 default_NN: Optional[int] = None,
                 default_k: int = 0,
                 default_n: Optional[int] = None,
                 default_m: Optional[int] = None,
                 default_q: Optional[int] = None,
                 default_du: Optional[int] = None,
                 default_dv: Optional[int] = None,
                 default_dt: Optional[int] = None,
                 default_unibd: int = 1,
                 default_w: Optional[int] = None,
                 default_rep: Optional[int] = None,
                 default_size_dt: Optional[int] = None,
                 default_c2_mode: str = "ring_embed",
                 default_skip_pk_rounding: bool = False,
                 default_codebits: Optional[int] = None,
                 default_errtolerance: int = 0):
        self.template = template
        self.name = name
        self.spec_params = spec_params
        self.default_q = default_q
        self.default_du = default_du
        self.default_dv = default_dv
        self.default_dt = default_dt
        self.default_unibd = default_unibd
        self.default_w = default_w
        self.default_rep = default_rep if default_rep is not None else self.template.default_rep
        self.default_size_dt = default_size_dt if default_size_dt is not None else getattr(template, "default_size_dt", None)
        self.default_c2_mode = default_c2_mode if default_c2_mode is not None else getattr(template, "default_c2_mode", "ring_embed")
        self.default_skip_pk_rounding = (
            default_skip_pk_rounding
            if default_skip_pk_rounding is not None
            else getattr(template, "default_skip_pk_rounding", False)
        )
        # 优先使用传入的值；若未传，则用 template/default_NN 作为codebits；errtolerance 默认为0
        self.default_k = default_k if default_k is not None else getattr(template, "default_k", 0)
        self.default_NN = default_NN if default_NN is not None else getattr(template, "default_NN", None)
        self.default_codebits = default_codebits if default_codebits is not None else (self.default_NN if self.default_NN is not None else getattr(template, "default_codebits", None))
        self.default_errtolerance = default_errtolerance if default_errtolerance is not None else getattr(template, "default_errtolerance", 0)

        if self.default_dt is None:
            self.default_dt = ceil(log2(self.default_q))

        if self.default_du is None:
            self.default_du = self.default_dt 

        if self.default_dv is None:
            self.default_dv = self.default_dt 

        # 优先使用传入的 default_k/NN，否则回退到模板上的值
        self.default_k = default_k if default_k is not None else getattr(template, "default_k", 0)
        self.default_NN = default_NN if default_NN is not None else getattr(template, "default_NN", None)

        # 如果 default_k != 0，则要求 default_NN 可用，并将 default_n/default_m 设为 default_k * default_NN
        if self.default_k != 0:
            if self.default_NN is None:
                raise ValueError("default_NN must be provided when default_k != 0")
            val = self.default_k * self.default_NN
            self.default_n = val
            self.default_m = val
        else:
            self.default_n = default_n
            self.default_m = default_m

    def build_ds(self,
                 spec_params: Optional[List[Any]] = None,
                 q: Optional[int] = None,
                 du: Optional[int] = None,
                 dv: Optional[int] = None,
                 dt: Optional[int] = None):
        """
        构造 DistributionSet。优先使用调用时传入的 spec_params/q/du/dv/dt，
        然后回退到实例绑定的默认值，再回退到模板（模板只包含 spec_meths）。
        """
        spec_params_use = spec_params if spec_params is not None else self.spec_params
        if spec_params_use is None:
            raise ValueError("spec_params must be provided to build_ds or bound on the instance")

        q_use = q if q is not None else self.default_q
        if q_use is None:
            raise ValueError("q must be provided to build_ds or bound on the instance")

        du_use = du if du is not None else self.default_du
        dv_use = dv if dv is not None else self.default_dv
        dt_use = dt if dt is not None else self.default_dt

        ds = build_ds_from_spec(
            q_use,
            self.template.spec_meths,
            spec_params_use,
            du=du_use,
            dv=dv_use,
            dt=dt_use,
            c2_mode=self.default_c2_mode,
            skip_pk_rounding=self.default_skip_pk_rounding,
        )
        return ds

    def compute_thres(self, q: int, w: int):
        return self.template.thres_fn(q, w)

    def calc_size(self,
                  seedbytes: Optional[int] = 32,
                  mbar: Optional[int] = None,
                  nbar: Optional[int] = None):
        """
        计算输出参数对应的尺寸（字节）。
        使用实例绑定的默认值：default_n、default_m、default_dt、default_du、default_dv。
        mbar 和 nbar 为必需参数（不在实例上绑定）。
        返回 (pkBytes, ctBytes)
        """
        # 使用实例默认值（不再从调用参数接收 n/m/dt/du/dv）
        n_use = self.default_n
        m_use = self.default_m
        if n_use is None or m_use is None:
            raise ValueError("n and m must be bound on the instance to use calc_size")

        dt_use = self.default_dt
        size_dt_use = self.default_size_dt if self.default_size_dt is not None else dt_use
        du_use = self.default_du
        dv_use = self.default_dv
        if size_dt_use is None or du_use is None or dv_use is None:
            raise ValueError("size_dt/du/dv must be bound on the instance to use calc_size")

        if self.default_k > 0: # m = n = k*NN.
            # 公钥尺寸必须使用“真实序列化位数”，不一定等于失败率分析中用于构造分布的 dt。
            pkBytes = m_use * size_dt_use // 8 + seedbytes
            ctBytes = (n_use * du_use + self.default_NN * dv_use) // 8
        else:
            if mbar is None or nbar is None:
                raise ValueError("mbar and nbar must be provided to calc_size")
            pkBytes = m_use * nbar * size_dt_use // 8 + seedbytes
            ctBytes = (n_use * du_use + nbar * dv_use) * mbar // 8
        
        return pkBytes, ctBytes

    def compute_failure(self,
                        n: Optional[int] = None,
                        m: Optional[int] = None,
                        w: Optional[int] = None,
                        spec_params: Optional[List[Any]] = None,
                        q: Optional[int] = None,
                        du: Optional[int] = None,
                        dv: Optional[int] = None,
                        dt: Optional[int] = None,
                        unibd: Optional[int] = None):
        """
        直接返回 failure_common 值。n 和 m 可选，若未传入则使用实例绑定的 default_n/default_m。
        w 默认为 1（可覆盖）。
        """
        n_use = n if n is not None else self.default_n
        m_use = m if m is not None else self.default_m
        if n_use is None or m_use is None:
            raise ValueError("n and m must be provided to compute_failure or bound on the instance")

        q_use = q if q is not None else self.default_q
        if q_use is None:
            raise ValueError("q must be provided to compute_failure or bound on the instance")

        # 选择 w：调用优先 -> 实例绑定 -> 模板绑定 -> 报错
        w_use = w if w is not None else (self.default_w if self.default_w is not None else self.template.default_w)
        if w_use is None:
            raise ValueError("w must be provided to compute_failure or bound on the instance/template")

        ds = self.build_ds(spec_params=spec_params, q=q_use, du=du, dv=dv, dt=dt)
        thres = self.compute_thres(q_use, w_use)
        unibd_use = unibd if unibd is not None else self.default_unibd
        return failure_common_rep(ds, n_use, m_use, thres, unibd_use, self.default_rep)
