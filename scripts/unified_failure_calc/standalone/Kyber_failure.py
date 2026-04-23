import operator as op
from math import factorial as fac
from math import sqrt, log
import sys
from proba_util import *

class KyberParameterSet:
    def __init__(self, n, m, ks, ke,  q, rqk, rqc, rq2, ke_ct=None):
        if ke_ct is None:
            ke_ct = ke
        self.n = n
        self.m = m
        self.ks = ks     # binary distribution for the secret key
        self.ke = ke    # binary distribution for the ciphertext errors
        self.ke_ct = ke_ct    # binary distribution for the ciphertext errors
        self.q = q
        self.rqk = rqk  # 2^(bits in the public key)
        self.rqc = rqc  # 2^(bits in the first ciphertext)
        self.rq2 = rq2  # 2^(bits in the second ciphertext)

def p2_cyclotomic_final_error_distribution(ps):
    """ construct the final error distribution in our encryption scheme
    :param ps: parameter set (ParameterSet)
    """
    chis = build_centered_binomial_law(ps.ks)           # LWE error law for the key: s/e
    chie = build_centered_binomial_law(ps.ke_ct)        # LWE error law for the ciphertext e1/e2
    chie_pk = build_centered_binomial_law(ps.ke)        # LWE error law for the ephemeral key: r
    Rk = build_mod_switching_error_law(ps.q, ps.rqk)    # Rounding error public key: ct
    Rc = build_mod_switching_error_law(ps.q, ps.rqc)    # rounding error first ciphertext: cu

    # chiRs = law_convolution(chis, Rk)                   # LWE+Rounding error key: (e + ct)
    chiRs = chis
    chiRe = law_convolution(chie, Rc)                   # LWE + rounding error ciphertext: (e1 + cu)

    B1 = law_product(chie_pk, chiRs)                    # (e + ct)*r
    B2 = law_product(chis, chiRe)                       # (e1 + cu)*s

    C1 = iter_law_convolution(B1, ps.m * ps.n)
    C2 = iter_law_convolution(B2, ps.m * ps.n)

    C=law_convolution(C1, C2)

    R2 = build_mod_switching_error_law(ps.q, ps.rq2)    # Rounding2 (in the ciphertext mask part): cv
    F = law_convolution(R2, chie)                       # LWE+Rounding2 error: (e2 + cv)
    D = law_convolution(C, F)                           # Final error
    return D


def p2_cyclotomic_error_probability(ps):
    F = p2_cyclotomic_final_error_distribution(ps)
    proba = tail_probability(F, ps.q/4)
    return F, ps.n*proba

def communication_costs(ps):
    """ Compute the communication cost of a parameter set
    :param ps: Parameter set (ParameterSet)
    :returns: (cost_Alice, cost_Bob) (in Bytes)
    """
    A_space = 256 + ps.n * ps.m * log(ps.rqk)/log(2)
    B_space = ps.n * ps.m * log(ps.rqc)/log(2) + ps.n * log(ps.rq2)/log(2)
    return (int(round(A_space))/8., int(round(B_space))/8.)


def summarize(ps):
    # print ("params: ", ps.__dict__)
    # print ("com costs: ", communication_costs(ps))
    F, f = p2_cyclotomic_error_probability(ps)
    print ("failure: %.1f = 2^%.1f"%(f, log(f + 2.**(-300))/log(2)))

if __name__ == "__main__":
    # Parameter sets
    ps_light = KyberParameterSet(256, 2, 3, 3, 3329, 2**12, 2**10, 2**4, ke_ct=2) #orig: 2^-139, <primal-pq:107>
    ps_recommended = KyberParameterSet(256, 3, 2, 2, 3329, 2**12, 2**10, 2**4) #orig: 2^-165, <primal-pq:166>
    ps_paranoid = KyberParameterSet(256, 4, 2, 2, 3329, 2**12, 2**11, 2**5) #orig: 2^-175, <primal-pq:232>

    # Analyses
    # print ("Kyber512 (light):")
    # summarize(ps_light)
    # print ()

    # print ("Kyber768 (recommended):")
    # summarize(ps_recommended)
    # print ()

    print ("Kyber1024 (paranoid):")
    summarize(ps_paranoid)
    print ()