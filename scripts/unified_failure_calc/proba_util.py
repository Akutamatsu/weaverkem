from math import factorial as fac
from math import log, ceil, erf, sqrt
from scipy.stats import norm, chi2

def gaussian_center_weight(sigma, t):
    """ Weight of the gaussian of std deviation s, on the interval [-t, t]
    :param x: (float)
    :param y: (float)
    :returns: erf( t / (sigma*\sqrt 2) )
    """
    return erf(t / (sigma * sqrt(2.)))


def binomial(x, y):
    """ Binomial coefficient
    :param x: (integer)
    :param y: (integer)
    :returns: y choose x
    """
    try:
        binom = fac(x) // fac(y) // fac(x - y)
    except ValueError:
        binom = 0
    return binom


def centered_binomial_pdf(k, x):
    """ Probability density function of the centered binomial law of param k at x
    :param k: (integer)
    :param x: (integer)
    :returns: p_k(x)
    """
    return binomial(2*k, x+k) / 2.**(2*k)


def build_centered_binomial_law(k):
    """ Construct the binomial law as a dictionnary
    :param k: (integer)
    :param x: (integer)
    :returns: A dictionnary {x:p_k(x) for x in {-k..k}}
    """
    D = {}
    for i in range(-k, k+1):
        D[i] = centered_binomial_pdf(k, i)
    return D

def build_ternary_law(alpha):
    """ Build a ternary distribution with P(-1) = P(1) = alpha/2, P(0) = 1 - alpha
    :param alpha: (float) parameter
    :returns: A dictionnary {x:p(x) for x in {-1,0,1}}
    """
    D = {}
    D[-1] = alpha/2.
    D[1] = alpha/2.
    D[0] = 1. - alpha
    return D

def mod_switch(x, q, rq):
    """ Modulus switching (rounding to a different discretization of the Torus)
    :param x: value to round (integer)
    :param q: input modulus (integer)
    :param rq: output modulus (integer)
    """
    return int(round(1.* rq * x / q) % rq)


def mod_centered(x, q):
    """ reduction mod q, centered (ie represented in -q/2 .. q/2)
    :param x: value to round (integer)
    :param q: input modulus (integer)
    """
    a = x % q
    if a < q/2:
        return a
    return a - q


def build_mod_switching_error_law(q, rq):
    """ Construct Error law: law of the difference introduced by switching from and back a uniform value mod q
    :param q: original modulus (integer)
    :param rq: intermediate modulus (integer)
    """
    D = {}
    V = {}
    for x in range(q):
        y = mod_switch(x, q, rq)
        z = mod_switch(y, rq, q)
        d = mod_centered(x - z, q)
        D[d] = D.get(d, 0) + 1./q
        V[y] = V.get(y, 0) + 1

    return D


def law_convolution(A, B):
    """ Construct the convolution of two laws (sum of independent variables from two input laws)
    :param A: first input law (dictionnary)
    :param B: second input law (dictionnary)
    """

    C = {}
    for a in A:
        for b in B:
            c = a+b
            C[c] = C.get(c, 0) + A[a] * B[b]
    return C


def law_product(A, B):
    """ Construct the law of the product of independent variables from two input laws
    :param A: first input law (dictionnary)
    :param B: second input law (dictionnary)
    """
    C = {}
    for a in A:
        for b in B:
            c = a*b
            C[c] = C.get(c, 0) + A[a] * B[b]
    return C


def clean_dist(A):
    """ Clean a distribution to accelerate further computation (drop element of the support with proba less than 2^-300)
    :param A: input law (dictionnary)
    """
    B = {}
    for (x, y) in A.items():
        if y>2**(-300):
            B[x] = y
    return B


def iter_law_convolution(A, i):
    """ compute the -ith forld convolution of a distribution (using double-and-add)
    :param A: first input law (dictionnary)
    :param i: (integer)
    """
    D = {0: 1.0}
    i_bin = bin(i)[2:]  # binary representation of n
    ctr = 0
    for ch in i_bin:
        D = law_convolution(D, D)
        D = clean_dist(D)
        if ch == '1':
            D = law_convolution(D, A)
            D = clean_dist(D)
        ctr += 1
    return D

def abs_distribution(distribution):
    """返回输入分布在绝对值坐标下的分布（将所有 |x| 相同的概率合并）。
    输入: distribution: dict {x: p}
    输出: dict {abs(x): sum p}
    """
    R = {}
    for x, p in distribution.items():
        a = abs(x)
        # 若绝对值为浮点且是整数形式，转换为 int 以便与整数键合并
        if isinstance(a, float) and a.is_integer():
            a = int(a)
        R[a] = R.get(a, 0) + p
    return R

def law_modq(distribution, q):
    """返回输入分布在模q坐标下的分布（将所有 x mod q 相同的概率合并）。
    输入: distribution: dict {x: p}
    输出: dict {x mod q: sum p}
    """
    R = {}
    for x, p in distribution.items():
        a = mod_centered(x, q)
        # 若绝对值为浮点且是整数形式，转换为 int 以便与整数键合并
        if isinstance(a, float) and a.is_integer():
            a = int(a)
        R[a] = R.get(a, 0) + p
    return R

def law_convolution_modq(A, B, q):
    C = {}
    for a in A:
        for b in B:
            c = mod_centered(a+b, q)
            C[c] = C.get(c, 0) + A[a] * B[b]
    return C

def law_square(A):
    """
    输入: A, dict, key 为取值，value 为概率
    输出: dict, key 为取值平方，value 为对应概率
    """
    D = {}
    for x, p in A.items():
        y = x ** 2
        if y in D:
            D[y] += p
        else:
            D[y] = p
    return D

# def iter_law_convolution_modq(A, i, q):
#     """ compute the -ith forld convolution of a distribution (using double-and-add)
#     :param A: first input law (dictionnary)
#     :param i: (integer)
#     """
#     D = {0: 1.0}
#     i_bin = bin(i)[2:]  # binary representation of n
#     ctr = 0
#     for ch in i_bin:
#         D = law_convolution_modq(D, D, q)
#         D = clean_dist(D)
#         if ch == '1':
#             D = law_convolution_modq(D, A, q)
#             D = clean_dist(D)
#         ctr += 1
#         # print(f"iter_law_convolution: finished {2**ctr}")
#     return D

def tail_probability(D, t):
    '''
    Probability that an drawn from D is strictly greater than t in absolute value
    :param D: Law (Dictionnary)
    :param t: tail parameter (integer)
    '''
    s = 0
    ma = max(D.keys())
    if t >= ma:
        return 0
    for i in reversed(range(int(ceil(t)), ma)):  # Summing in reverse for better numerical precision (assuming tails are decreasing)
        s += D.get(i, 0) + D.get(-i, 0)
    return s

def norm_pdf(x, mu, sd):
    return norm.pdf(x, loc=mu, scale=sd)

def approx_discrete_gaussian(sd):
    half = ceil(4 * sd)
    D = {}
    sum = 0
    for x in range(-half, half+1):
        sum += norm_pdf(x, 0, sd)
        D[x] = norm_pdf(x, 0, sd)

    for x in D:
        D[x] /= sum
    return D

def get_mean(distribution):
    return sum(x * p for x, p in distribution.items())

def get_variance(distribution):
    # distribution: a dictionary where keys are data points and values are their probabilities
    # Step 1: Calculate E[X]
    mu = get_mean(distribution)
    
    # Step 2: Calculate E[X^2]
    mu_squared = sum(x**2 * p for x, p in distribution.items())
    
    # Step 3: Calculate variance
    var = mu_squared - mu**2
    return var

def get_deviation(distribution):
    return sqrt(get_variance(distribution))

def build_uniform_law(a, b):
    """ Construct the uniform law as a dictionnary
    :param a: (integer)
    :param b: (integer)
    :returns: A dictionnary {x: 1/(b-a+1) for x in {a..b}}
    """
    D = {}
    for i in range(a, b+1):
        D[i] = 1 / (b - a + 1)
    return D

def build_asymmetric_error_law(q, dv):
    D = {}
    power_dv = 2 ** dv
    for x in range(q):
        y = mod_switch(x, q, power_dv)
        z = mod_switch(y, power_dv, q) 
        e0 = mod_centered(z - x, q)
        y1 = y + power_dv/2
        z1 = mod_switch(y1, power_dv, q)
        e1 = mod_centered(z1 - x - (q-1)//2, q)
        e = e0 + e1
        # D[e0] = D.get(e0, 0) + 1./(2*q)
        # D[e] = D.get(e, 0) + 1./(2*q)
        D[e] = D.get(e, 0) + 1./(q)
    return D
