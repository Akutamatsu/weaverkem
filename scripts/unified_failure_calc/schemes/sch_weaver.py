from schemes.template.scheme_template import SchemeTemplate

# 模板：WeaverTemplateL1
WeaverTemplateL1 = SchemeTemplate(
    name="WeaverTemplateL1",
    spec_meths={'s':'cbd', 'r':'cbd', 'e':'na', 'ep':'na', 'epp':'na'},
    thres_fn=lambda q,w: q // (2 ** (w + 1)),
    default_unibd=1,
    default_NN=256,
    default_w=1,
    default_rep=1, # no repetition code.
)

Weaver128 = WeaverTemplateL1.instantiate(
    inst_name="Weaver128",
    spec_params=[2, 2, 0,0,0], # failure only depends on here, not on k.
    default_NN=256,
    default_k=2,
    default_q=3329,
    default_du=8,
    default_dv=4,
    default_dt=9, # 8
    default_unibd=1,
    default_codebits=128+24, # 24 bits for BCH (255,3)
    default_errtolerance=4 # correct up to 4 bits error.
)

# sgWeaver512 = WeaverTemplateL1.instantiate(
#     inst_name="sgWeaver512",
#     spec_params=[1, 1, 0,0,0],
#     default_NN=1024,
#     default_k=2,
#     default_q=7681, # larger q for same level of NTT: mod degree 4 poly.
#     default_du=9,
#     default_dv=6,
#     default_dt=9,
#     default_unibd=1,
#     default_codebits=512+40, # 40 bits for BCH (1023,4)
#     default_errtolerance=4 # correct up to 4 bits error.
# )

# 模板：WeaverTemplateL2
WeaverTemplateL2 = SchemeTemplate(
    name="WeaverTemplateL2",
    spec_meths={'s':'cbd', 'r':'cbd', 'e':'na', 'ep':'na', 'epp':'na'},
    thres_fn=lambda q,w: q // (2 ** (w + 1)),
    default_unibd=1,
    default_NN=256,
    default_w=2,
    default_rep=4, # for D4, rep = 4.
    # default_rep=2, # for D4, rep = 4.
)

Weaver256 = WeaverTemplateL2.instantiate(
    inst_name="Weaver256",
    spec_params=[2, 2, 0,0,0],
    default_NN=256,
    default_k=4,
    default_q=3329,
    default_du=9,
    default_dv=4,
    default_dt=10, # 9
    default_unibd=1,
    default_codebits=64,
    default_errtolerance=3
    # default_errtolerance=3 # correct up to 3 bits error.
)
Weaver256L1 = WeaverTemplateL1.instantiate(
    inst_name="Weaver256L1",
    spec_params=[2, 2, 0,0,0],
    default_NN=256,
    default_k=4,
    default_q=3329,
    default_du=9,
    default_dv=4,
    default_dt=10, # 9
    default_unibd=1,
    default_codebits=231+24,
    default_errtolerance=3 # correct up to 3 bits error.
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
    spec_params=[1, 1, 0,0,0],
    default_NN=512,
    default_k=4,
    default_q=3329,
    default_du=9,
    default_dv=6,
    default_dt=10, # 9
    default_unibd=1,
    default_codebits=40+28, # 28 bits for BCH (127,4)
    default_errtolerance=4 # correct up to 4 bits error.
)
Weaver512L1 = WeaverTemplateL1.instantiate(
    inst_name="Weaver512L1",
    spec_params=[1, 1, 0,0,0],
    default_NN=512,
    default_k=4,
    default_q=3329,
    default_du=9,
    default_dv=6,
    default_dt=10, # 9
    default_unibd=1,
    default_codebits=470+36, # 36 bits for BCH (511,4)
    default_errtolerance=4 # correct up to 4 bits error.
)

'''
[4-bits correction Weaver512]
Weaver512 failure: 1.325e-20 = 2^-66.03
After correcting 4 bits from total 68, failure prob is 2^-306.63
  Public key size: 2336
  Ciphertext size: 2688
Weaver512L1 failure: 1.406e-21 = 2^-69.27
After correcting 4 bits from total 506, failure prob is 2^-308.33
  Public key size: 2336
  Ciphertext size: 2688
    ==============================
[Classical; biased-LWR]
root@zzd-virtual-machine:/mnt/hgfs/VM_share/lattice-estimator-main# sage my_estimate.sage
Running..
LWEParameters(n=2048, q=3329, Xs=D(σ=0.71), Xe=D(σ=1.90), m=2048, tag='Weaver512')
Algorithm <estimator.lwe_bkw.CodedBKW object at 0x74ffb3d69c00> on LWEParameters(n=2048, q=3329, Xs=D(σ=0.71), Xe=D(σ=1.90), m=2048, tag='Weaver512') failed with Calling ceil() on infinity or NaN
usvp                 :: rop: ≈2^572.6, red: ≈2^572.6, δ: 1.001212, β: 1961, d: 3250, tag: usvp
bdd                  :: rop: ≈2^572.8, red: ≈2^572.0, svp: ≈2^571.4, β: 1959, η: 1957, d: 3289, tag: bdd
dual                 :: rop: ≈2^588.4, mem: ≈2^429.1, m: 1460, β: 2015, d: 3508, ↻: 1, tag: dual

'''



'''
Weaver128 failure: 1.240e-10 = 2^-32.91
After correcting 4 bits from total 152, failure prob is 2^-135.21
  Public key size: 544
  Ciphertext size: 640
Weaver256 failure: 4.833e-16 = 2^-50.88
After correcting 3 bits from total 64, failure prob is 2^-184.10
  Public key size: 1184
  Ciphertext size: 1280
Weaver512 failure: 3.191e-28 = 2^-91.34
After correcting 3 bits from total 64, failure prob is 2^-345.94
  Public key size: 2592
  Ciphertext size: 2816
Weaver256L1 failure: 1.541e-18 = 2^-59.17
After correcting 3 bits from total 255, failure prob is 2^-209.29
  Public key size: 1184
  Ciphertext size: 1280
Weaver512L1 failure: 1.003e-33 = 2^-109.62
After correcting 3 bits from total 508, failure prob is 2^-407.11
  Public key size: 2592
  Ciphertext size: 281
  ==============================
[Classical; biased-LWR]
root@zzd-virtual-machine:/mnt/hgfs/VM_share/lattice-estimator-main# sage my_estimate.sage
Running..
LWEParameters(n=512, q=3329, Xs=D(σ=1.00), Xe=D(σ=3.74), m=512, tag='Weaver128')
bkw                  :: rop: ≈2^180.1, m: ≈2^166.6, mem: ≈2^167.6, b: 14, t1: 0, t2: 14, ℓ: 13, #cod: 439, #top: 2, #test: 71, tag: coded-bkw
usvp                 :: rop: ≈2^136.9, red: ≈2^136.9, δ: 1.003562, β: 469, d: 944, tag: usvp
bdd                  :: rop: ≈2^137.9, red: ≈2^136.4, svp: ≈2^137.2, β: 467, η: 470, d: 955, tag: bdd
dual                 :: rop: ≈2^143.4, mem: ≈2^110.8, m: 494, β: 491, d: 1006, ↻: 1, tag: dual
dual_hybrid          :: rop: ≈2^131.2, red: ≈2^131.1, guess: ≈2^126.5, β: 449, p: 4, ζ: 10, t: 40, β': 449, N: ≈2^92.5, m: 512

LWEParameters(n=1024, q=3329, Xs=D(σ=1.00), Xe=D(σ=1.90), m=1024, tag='Weaver256')
bkw                  :: rop: ≈2^331.5, m: ≈2^318.7, mem: ≈2^319.7, b: 27, t1: 0, t2: 14, ℓ: 26, #cod: 876, #top: 0, #test: 149, tag: coded-bkw
usvp                 :: rop: ≈2^277.4, red: ≈2^277.4, δ: 1.002124, β: 950, d: 1814, tag: usvp
bdd                  :: rop: ≈2^278.0, red: ≈2^277.1, svp: ≈2^276.8, β: 949, η: 948, d: 1849, tag: bdd
dual                 :: rop: ≈2^287.0, mem: ≈2^213.9, m: 900, β: 983, d: 1924, ↻: 1, tag: dual
dual_hybrid          :: rop: ≈2^261.5, red: ≈2^260.8, guess: ≈2^260.3, β: 893, p: 4, ζ: 0, t: 120, β': 893, N: ≈2^184.8, m: 1024

LWEParameters(n=2048, q=3329, Xs=D(σ=1.00), Xe=D(σ=0.96), m=2048, tag='Weaver512')
Algorithm <estimator.lwe_bkw.CodedBKW object at 0x78d449d85c90> on LWEParameters(n=2048, q=3329, Xs=D(σ=0.96), Xe=D(σ=1.00), m=2048, tag='Weaver512') failed with Calling ceil() on infinity or NaN
usvp                 :: rop: ≈2^556.3, red: ≈2^556.3, δ: 1.001240, β: 1905, d: 3583, tag: usvp
bdd                  :: rop: ≈2^557.1, red: ≈2^555.4, svp: ≈2^556.6, β: 1902, η: 1906, d: 3620, tag: bdd
dual                 :: rop: ≈2^572.9, mem: ≈2^418.1, m: 1647, β: 1962, d: 3695, ↻: 1, tag: dual
dual_hybrid          :: rop: ≈2^545.4, red: ≈2^544.6, guess: ≈2^544.2, β: 1753, p: 3, ζ: 0, t: 330, β': 1753, N: ≈2^396.5, m: ≈2^11.0

'''