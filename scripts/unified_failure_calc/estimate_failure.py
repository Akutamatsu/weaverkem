'''
# 版本: v2.1.0
# 修改日期: 2026-04-23
# 作者: zzd
# 描述: 该模块提供了一个工厂函数 get_preset(name) 来获取预定义的 scheme 实例，
#       以及一个 get_preset_schemes() 函数来获取所有预定义 scheme 的字典。
#       用户可以基于这些实例快速计算失败概率和大小。
#       当用户需要自定义新参数集时，可在 schemes 文件夹下添加新 python 文件并自定义参数后，
#       在本文件中 import 并包含到 _PRESETS 字典中，运行本文件将计算所有 _PRESETS 字典中包含的未注释 scheme。
'''

from typing import Any
from binomial import est_after_correcting_k_bits

# from schemes.sch_smaug import SMAUGT128, SMAUGT192, SMAUGT256
# from schemes.sch_frodo import Frodo640
# from schemes.sch_scloudplus import Scloudplus128, Scloudplus192, Scloudplus256
# from schemes.sch_kyber import Kyber512, Kyber768, Kyber1024
# from schemes.sch_saber import LightSaber, Saber, FireSaber
# from schemes.sch_lac import LAC128, LAC192, LAC256

### 【提交版参数】
from schemes.sch_weaver_inv import Weaver128L1, Weaver128L2, Weaver256L1, Weaver256L2, Weaver512L1, Weaver512L2
from schemes.sch_loom import Loom128, Loom256, Loom512

# 工厂字典便于 lookup
_PRESETS = {
    "Loom128": Loom128,
    "Loom256": Loom256,
    "Loom512": Loom512,
    # "Weaver128L1": Weaver128L1,
    # "Weaver128L2": Weaver128L2,
    # "Weaver256L1": Weaver256L1,
    # "Weaver256L2": Weaver256L2,
    # "Weaver512L1": Weaver512L1,
    # "Weaver512L2": Weaver512L2,
    # "LAC128": LAC128,
    # "LAC192": LAC192,
    # "LAC256": LAC256,
    # "LightSaber": LightSaber,
    # "Saber": Saber,
    # "FireSaber": FireSaber,
    # "Frodo640": Frodo640,
    # "Kyber512": Kyber512,
    # "Kyber768": Kyber768,
    # "Kyber1024": Kyber1024,
    # "SMAUGT128": SMAUGT128,
    # "SMAUGT192": SMAUGT192,
    # "SMAUGT256": SMAUGT256,
}

def get_preset(name: str) -> Any:
    if name not in _PRESETS:
        raise KeyError(f"preset '{name}' not found")
    return _PRESETS[name]

def get_preset_schemes() -> dict:
    """
    返回预定义的 scheme 实例字典，用户可基于此快速查找并 compute_failure。
    """
    return _PRESETS.copy()

from math import log

if __name__ == "__main__":

    testSchemes = get_preset_schemes()
    # print (ceil(log2(15361//20)))

    '''for Ring/MLWE-based scheme:''' 
    for name, scheme in testSchemes.items():
        f = scheme.compute_failure()
        print(f"{name} failure: %.3e = 2^%.2f"%(f, log(f + 2.**(-300))/log(2)))
        if hasattr(scheme, "default_bch_n"):
            print(
                f"  BCH: ({scheme.default_bch_n}, {scheme.default_bch_k}, {scheme.default_bch_t}), "
                f"encoded_bits={scheme.default_codebits}, payload_bits={scheme.default_payload_bits}"
            )
        # if hasattr(scheme, "capacity_summary"):
        #     print(f"  Capacity check: {scheme.capacity_summary}")
        if hasattr(scheme, "validation_issues") and scheme.validation_issues:
            for issue in scheme.validation_issues:
                print(f"  Warning: {issue}")
        if scheme.default_unibd == 1:
            est_after_correcting_k_bits(scheme.default_codebits, f, scheme.default_errtolerance) # print failure probability after error correction
        pkBytes, ctBytes = scheme.calc_size() # for Ring/MLWE-based scheme
        print(f"  Public key size: {pkBytes}")
        print(f"  Ciphertext size: {ctBytes}")
        

    '''for LWE-based scheme:''' 
    # mnbars = [[10,8], [13,10]]
    # for (name, scheme), (mbar, nbar) in zip(testSchemes.items(), mnbars):
    #     f = scheme.compute_failure()
    #     print(mbar, nbar)
    #     print(f"{name} failure: %.3e = 2^%.2f"%(f, log(f + 2.**(-300))/log(2)))
    #     pkBytes, ctBytes = scheme.calc_size(mbar=mbar, nbar=nbar) # for LWE-based scheme
    #     print(f"  Public key size: {pkBytes}")
    #     print(f"  Ciphertext size: {ctBytes}")
        
    # 使用示例：直接用 Kyber512 实例计算
    # f = Kyber512.compute_failure()
    # print("Kyber512 failure: %.3e = 2^%.2f"%(f, log(f + 2.**(-300))/log(2)))

    # # 若需要临时覆盖参数也可以：
    # f3 = Kyber512.compute_failure(dv=3)
    # print("Kyber512 (explicit) failure: %.3e = 2^%.2f"%(f3, log(f3 + 2.**(-300))/log(2)))

    # 使用示例：用 SMAUGT256 实例计算
    # f2 = SMAUGT256.compute_failure()
    # print("SMAUGT256 failure: %.3e = 2^%.2f"%(f2, log(f2 + 2.**(-300))/log(2)))