from proba_util import *
from failure_common import *
import scipy.special

def failure_prob_by_distr(noise, n, thres):
    B = law_product(noise, noise)
    D = iter_law_convolution(B, 2*n)
    F = law_convolution(D, noise)  # D = 2n * (noise^2) + noise
    proba = tail_probability(F, thres)
    unibd = 64
    return unibd * proba

def failure_prob_by_sd(sd, n, thres):
    noise = approx_discrete_gaussian(sd)
    B = law_product(noise, noise)
    D = iter_law_convolution(B, 2*n)
    F = law_convolution(D, noise)  # D = 2n * (noise^2) + noise
    proba = tail_probability(F, thres)
    unibd = 64
    return unibd * proba

def dist_frodo(n):
    if n == 640:
        T = [9288, 8720, 7216, 5264, 3384, 1918, 958, 422, 164, 56, 17, 4, 1]
    elif n == 976:
        T = [11278, 10277, 7774, 4882, 2545, 1101, 396, 118, 29, 6, 1]
    elif n == 1344:
        T = [18286, 14320, 6876, 2023, 364, 40, 2]
    else:
        raise ValueError("Unsupported n")
    D = {}
    for (i, j) in enumerate(T):
        D[i] = 2**-16 * j
        D[-i] = 2**-16 * j
    return D

sd_frodo = {640:2.8, 976:2.3, 1344:1.4}

def failure_prob_frodo(n, m, q, unibd, w):
    # meths = ["gaussian", None, None, None, None]
    # sd = sd_frodo[n]
    # params = [sd, 0,0,0,0]
    meths = ["define", None, None, None, None]
    noise = dist_frodo(n)
    params = [noise, 0,0,0,0]
    ds = DistributionSet(meths, params, q)

    thres = q / 2 ** (w + 1)
    f = failure_common(ds, n, m, thres, unibd)
    return f

def failure_prob_kyber(n, k, q, eta1, eta2, dt, du, dv, unibd = 1):
    meths = ["cbd", None, None, "cbd", None]
    params = [eta1, 0, 0, eta2, 0]
    ds = DistributionSet(meths, params, q, du, dv)

    dim = k*n
    thres = q / 4
    # thres = q / 8
    f = failure_common(ds, dim, dim, thres, unibd)
    return f

def failure_prob_kyber_ex(n, k, q, sigma, dt, du, dv, unibd = 1):
    meths = ["gaussian", None, None, None, None]
    params = [sigma, 0, 0, 0, 0]
    ds = DistributionSet(meths, params, q, du, dv)

    dim = k*n
    # thres = q / 4
    thres = q / 8
    f = failure_common(ds, dim, dim, thres, unibd)
    return f

def failure_prob_scloud(m, n, q, eta1, eta2, du, dv, mbar, nbar, ssbits, mu, tau, thres):
    hws = n // 2
    hwr = m // 2
    meths = ["ternary", "ternary", "cbd", "cbd", "cbd"]
    params = [hws, hwr, eta1, eta2, eta2]
    ds = DistributionSet(meths, params, q, du, dv)

    unibd = mbar * nbar
    f = failure_common(ds, n, m, thres, unibd)
    return f

def calc_size(m, n, q, dt, du, dv, mbar, nbar, w):
    pkBytes = m * nbar * dt // 8 + 32
    ctBytes = (n * du + nbar * dv) * mbar // 8

    return pkBytes, ctBytes

def failure_prob_scloud_bw32(m, n, q, eta1, eta2, du, dv, mbar, nbar, ssbits, mu, tau):
    hws = n // 2
    hwr = m // 2
    h1 = hws // 2
    h2 = hwr // 2
    logq = log2(q)

    var2 = eta1 * h2 + eta2 *(h1 +0.5) + ( 2**(2*(logq-dv))+2+ 2*h1 * 2**(2*(logq-du)) - 2*h1)/12
    a = 2**(2*logq - 2*tau+1)/var2

    f = ssbits/mu * scipy.special.gammaincc(16, a) # already: / scipy.special.gamma(16)
    return f

def failure_prob_frodo_ex(m, n, q, eta1, du, dv, unibd, w):
    hws = n // 2
    hwr = m // 2
    meths = ["ternary", "ternary", "cbd", "na", "na"]
    params = [hws, hwr, eta1, 0, 0]
    ds = DistributionSet(meths, params, q, du, dv)

    thres = q / 2 ** (w + 1)
    f = failure_common(ds, n, m, thres, unibd)
    return f

def failure_prob_frodo_ex2(m, n, q, dt, du, dv, mbar, nbar, w, rho_hw = 0.5, setunibd = 0):
    hws = ceil(n * rho_hw)
    hwr = ceil(m * rho_hw)
    meths = ["ternary", "ternary", "na", "na", "na"]
    params = [hws, hwr, 0, 0, 0]
    ds = DistributionSet(meths, params, q, du, dv, dt)

    unibd = mbar * nbar
    if setunibd:
        unibd = setunibd
    # thres = 2 ** (dt - w - 1)
    thres = q / 2 ** (w + 1)
    # thres = q / 2 ** (w )
    f = failure_common(ds, n, m, thres, unibd)
    return f

# def failure_prob_frodo_ex2(m, n, q, dt, du, dv, mbar, nbar, w, rho_hw = 0.5, setunibd = 0):
#     hws = ceil(n * rho_hw)
#     hwr = ceil(m * rho_hw)
#     meths = ["ternary", "ternary", "cbd", "na", "na"]
#     cbd_eta = {1:2, 2:4, 3:8}
#     params = [hws, hwr, cbd_eta.get(dt-du, 0), 0, 0]
#     ds = DistributionSet(meths, params, q, du, dv, dt)

#     unibd = mbar * nbar
#     if setunibd:
#         unibd = setunibd
#     thres = 2 ** (dt - w - 1)
#     # thres = q / 2 ** (w + 1)
#     f = failure_common(ds, n, m, thres, unibd)
#     return f

def get_SU_deviation(eta):
    D = uniform_distr(-eta, eta)
    E = law_convolution(D, D)
    # print (E)
    sig = get_deviation(E)
    print (sig)

def kyber_size(n, k, q, eta1, eta2, dt, du, dv):
    dim = n*k
    pkBytes = dim * dt // 8
    ctBytes = dim * du // 8 + n * dv // 8

    return pkBytes, ctBytes

def print_twos_exponent(text, value):
    print (text + " %.1f = 2^%.1f"%(value, log(value + 2.**(-300))/log(2)))

if __name__ == "__main__":

    # get_SU_deviation(4)
    # get_SU_deviation(2)


    '''
    n, n, q, 64, w
    '''
    frodo_ps1 = [640, 640, 2**15, 64, 2]
    frodo_ps2 = [976, 976, 2**16, 64, 3]
    frodo_ps3 = [1344, 1344, 2**16, 64, 4]

    # n = 1264
    # w = 4
    # q = 32257 # in (2^14, 2^15)
    # sd = 2.0

    # f = failure_prob_frodo(*frodo_ps1)
    # print ("failure: %.1f = 2^%.1f"%(f, log(f + 2.**(-300))/log(2)))

    # kyb_ps1 = [256, 2, 3329, 3, 2, 12, 10, 4]
    # kyb_ps2 = [256, 3, 3329, 2, 2, 12, 10, 4]
    # kyb_ps3 = [256, 4, 3329, 2, 2, 12, 11, 5]

    # kyb_ps1 = [256, 2, 3329, 3, 2, 12, 9, 3]
    # kyb_ps2 = [256, 3, 3329, 2, 2, 12, 7, 3]
    # kyb_ps3 = [256, 4, 3329, 2, 2, 12, 9, 4]

    # kyb_ps1 = [256, 2, 7681, 5, 5, 13, 10, 4]
    # kyb_ps2 = [256, 3, 7681, 4, 4, 13, 10, 4]
    # kyb_ps3 = [256, 4, 7681, 3, 3, 13, 10, 5]

    # kyb_ps1 = [256, 2, 769, 4, 4, 10, 10, 4] # toy example * single fail: 2^-8.2; 256 uni-fail ~= 1- 0.418
    # kyb_ps2 = [256, 3, 769, 2, 2, 10, 7, 4]  # toy example * single fail: 2^-8.4; 256 uni-fail ~= 1- 0.418
    # kyb_ps3 = [256, 4, 769, 2, 2, 10, 8, 4]

    # kyb_ps1 = [256, 2, 769, 1, 1, 10, 9, 4]
    # kyb_ps2 = [256, 3, 769, 1, 1, 10, 9, 3]
    # kyb_ps3 = [256, 4, 769, 1, 1, 10, 9, 3]

    # kyb_ps1 = [128, 4, 769, 1, 1, 10, 9, 4]
    # kyb_ps2 = [128, 6, 769, 1, 1, 10, 9, 3]
    # kyb_ps3 = [128, 8, 769, 1, 1, 10, 9, 3]

    kyb_ps1 = [256, 2, 2**10, 2.58, 10, 7, 7] 
    kyb_ps2 = [256, 2, 2**11, 2.58, 11, 8, 8] 
    kyb_ps3 = [256, 2, 2**12, 2.58, 12, 9, 9] 

    # kyb_ps1 = [256, 2, 2**10, 1.41, 10, 8, 8] 
    # kyb_ps2 = [256, 2, 2**11, 1.41, 11, 9, 9] 
    # kyb_ps3 = [256, 2, 2**12, 1.41, 12, 10, 10] # better

    kyb_ps1 = [256, 2, 2**10, 1.41, 10, 8, 8]
    kyb_ps2 = [256, 2, 2**12, 2.58, 12, 9, 9] # sec+, fail+
    kyb_ps3 = [256, 2, 2**11, 1.41, 11, 9, 9] # sec-, fail-

    # schemes = [kyb_ps1, kyb_ps2, kyb_ps3]
    schemes = [kyb_ps3]

    # for param_set in schemes:
    #     f = failure_prob_kyber_ex(*param_set, unibd = 1)
    #     print ("failure: %.1f = 2^%.1f"%(f, log(f + 2.**(-300))/log(2)))
    #     # pksize, ctsize = kyber_size(*param_set)
    #     # print(f"{pksize}, {ctsize}")

    # '''
    # m, n, q, hws, hwr, eta1, eta2, du, dv, unibd
    # '''
    q = 2**12
    # th1 = q / 2 ** (2 + 1)
    # th2 = q / 2 ** (3 + 1)
    # th3 = q / 2 ** (2 + 1)
    sclo_ps1 = [600,  600, q, 7,7,  9, 7, 8,8  ,128,64,3]
    sclo_ps2 = [928,  896, q, 2,1, 12,10, 8,8  ,192,96,4]
    sclo_ps3 = [1136,1120, q, 3,2, 10, 7, 12,11,256,64,3]

    # f = failure_prob_scloud(*sclo_ps3, th3)
    # print ("failure: %.1f = 2^%.1f"%(f, log(f + 2.**(-300))/log(2)))

    # f = failure_prob_scloud_bw32(*sclo_ps1)
    # print ("failure: %.1f = 2^%.1f"%(f, log(f + 2.**(-300))/log(2)))
    # f = failure_prob_scloud_bw32(*sclo_ps2)
    # print ("failure: %.1f = 2^%.1f"%(f, log(f + 2.**(-300))/log(2)))
    # f = failure_prob_scloud_bw32(*sclo_ps3)
    # print ("failure: %.1f = 2^%.1f"%(f, log(f + 2.**(-300))/log(2)))

    my_ps1 = [560,  560, 2**12, 7,  9, 7, 64, 2]
    my_ps2 = [900,  900, 2**13, 2, 10, 7, 100, 2]
    my_ps3 = [1264,1264, 2**14, 3, 10, 7, 144, 2]

    # my_ps2 = [976,  976, 2**16, 448, 464, 2, 10, 7, 100,2]
    # my_ps3 = [1344,1344, 2**16, 560, 568, 3, 10, 7, 132,2]

    # with sparse secret
    # f = failure_prob_frodo_ex(*my_ps1)
    # print ("failure: %.1f = 2^%.1f"%(f, log(f + 2.**(-300))/log(2)))
    # f = failure_prob_frodo_ex(*my_ps2)
    # print ("failure: %.1f = 2^%.1f"%(f, log(f + 2.**(-300))/log(2)))
    # f = failure_prob_frodo_ex(*my_ps3)
    # print ("failure: %.1f = 2^%.1f"%(f, log(f + 2.**(-300))/log(2)))

    # my_ps1 = [600,  600, 2**12, 9,  9, 7, 8,8,2] # 143,130; 2blk cor: 2^-164
    # my_ps3 = [1168,1168, 2**13, 10, 10,7, 12,12,2] # 284,258;

    # my_ps1 = [632,  632, 2**12, 9,  9, 7, 8,8,2] # 152,138; 2^-51.2

    # my_ps2 = [848,  848, 2**13, 10, 10,7, 10,10,2] # 196,178

    # my_ps2 = [760,  760, 2**12, 9, 9,8, 10,10,2] # 185,168
    # my_ps3 = [1200,1200, 2**13, 10, 10,7, 12,12,2] # 294,267; 2^-108.8

    # my_ps2 = [760,  760, 2**12, 9, 9,8, 13,14,2] # 189,171; 2^-48.6
    # my_ps3 = [1200,1200, 2**12, 10, 10,7, 16,17,2] # 297,269; 2^-106.4
    # # my_ps3 = [1200,1200, 2**13, 10, 10,7, 16,17,2] # 294,267

    # my_ps2a = [160,  160, 2**12, 8, 8, 7, 11,12,3] # Toy
    # my_ps2a = [840,  840, 2**13, 10, 10, 7, 11,11,3] # 194,176; 2^-42.0
    # my_ps2b = [872,  872, 2**13, 11, 11, 9, 11,11,3] # 189,171; 2^-149.0
    # my_ps3 = [1360,1360, 2**14, 12, 12, 8, 11,12,4] # 294,267; 2^-89.5
    # my_ps2b = [856, 856, 2**12, 11, 11, 8, 10, 11, 3] # 187,170; 2^-111.8
    # my_ps3 = [1360,1360, 2**14, 12, 12, 10, 11,12,4] # 294,267; 2^-96.1

    my_ps2b = [872, 872, 2**12, 10, 10, 8, 13, 13, 2] # 2^-149.8
    my_ps3 = [1376,1376, 2**13, 11, 11, 8, 13,14, 3] # 2^-93.1

    # ## NIST levels:
    # # my_ps2a = [584, 584, 2**10, 9,  9,  8, 9, 9, 2] # 143,130
    # my_ps2a = [600, 600, 2**11, 9,  9,  7, 9, 9, 2] # 144,131
    # # my_ps2b = [960, 960, 2**12, 11, 11, 8, 8, 9, 3] # 214,194
    # my_ps2b = [960, 960, 2**12, 11, 11, 10, 8, 9, 3] # 214,194
    # my_ps3 = [1320,1320, 2**14, 12, 12, 8, 8, 9, 4] # 284,258

    # my_ps2a = [832,  832, 2**13, 13, 10, 8, 11,12,3] # 189,172; 2^-50.9
    # my_ps2b = [960,  960, 2**14, 14, 12, 10, 11,11,4] # 189,171; 2^-149.0
    # my_ps3 = [1280,1280, 2**14, 14, 11, 8, 13, 14, 3] # 292,265; 2^-125

    ## Toy Parameter
    # my_ps2a = [160,  160, 2**12, 8, 8, 7, 11,12,3] # Toy
    my_ps2a = [800,800, 2**13, 9, 9, 6, 13,14, 3] # Toy

    # schemes = [my_ps2a, my_ps2b, my_ps3]

    # for param_set in schemes:
    #     f = failure_prob_frodo_ex2(*param_set, 0.5, 1)
    #     print ("failure: %.1f = 2^%.1f"%(f, log(f + 2.**(-300))/log(2)))
    #     pksize, ctsize = calc_size(*param_set)
    #     print(f"{pksize}, {ctsize}")

