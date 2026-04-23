from scipy.stats import binom
from math import log, comb

def log2(val):
    return log(val + 2.**(-300))/log(2)

def min_correction_bits(blks, coeff_fail):
    n = blks
    p = coeff_fail
    for k in range(0, 6):
        tail_value = binom.sf(k, n, p) # tail cumulative function
        print(f"The SF at k={k} (fail bit >) is 2^{log2(tail_value):.2f}")

def iteration_times(blks, corblks):
    times = 0
    for i in range(1, corblks+1):
        times += comb(blks, i)

    print(f"iteration times, correcting '{corblks}' from total '{blks}': {times}")


def log_poisson_estimation(k, n, p):
    '''
    log(prob) = (k+1)*(log(n)+log(p))-sum{i from 1 to t}(log(i+1))
    '''
    tmp = sum(log2(i+1) for i in range(1, k+1))
    val = (k+1)*(log2(n)+log2(p))-tmp
    return val

def after_correcting_k_bits(blks, coeff_fail, k):
    n = blks
    p = coeff_fail
    tail_value = binom.sf(k, n, p) # tail cumulative function
    print(f"After correcting {k} bits from total {blks}, failure prob is 2^{log2(tail_value):.2f}")

def est_after_correcting_k_bits(blks, coeff_fail, k):
    ''' can be used for result < 2^-300 '''
    n = blks
    p = coeff_fail
    tail_value = log_poisson_estimation(k, n, p) # using poisson_estimation
    print(f"After correcting {k} bits from total {blks}, failure prob is 2^{tail_value:.2f}")

if __name__ == "__main__":

    # p = 2**-(149.8)
    # min_correction_bits(169, p)
    # p = 2**-(93.1)
    # min_correction_bits(182, p)

    # # Toy:
    # p = 2**-(12.5)
    # min_correction_bits(164, p)

    # p = 2**(-39.22)
    # p = 2**(-43.62)
    # after_correcting_k_bits(128, p, 3)
    # p = 2**(-54.87)
    # after_correcting_k_bits(256, p, 3)
    # p = 2**(-83.10)
    # after_correcting_k_bits(512, p, 3)

    ## LWR
    # # p = 2**(-43.00)
    # p = 2**(-35.91)
    # after_correcting_k_bits(128, p, 4)
    # # p = 2**(-63.28)
    # p = 2**(-60.3)
    # after_correcting_k_bits(256, p, 3)


    # p = 2**(-39.4)
    # after_correcting_k_bits(128, p, 3)
    # est_after_correcting_k_bits(128, p, 3)
    # p = 2**(-18.1)
    # after_correcting_k_bits(63, p, 7)
    # p = 2**(-28.05)
    # after_correcting_k_bits(63, p, 5)
    # est_after_correcting_k_bits(63, p, 5)
    # p = 2**(-22)
    # after_correcting_k_bits(32, p, 1)
    # p = 2**(-29.4)
    # after_correcting_k_bits(64, p, 5)
    # p = 2**(-60.94)
    # after_correcting_k_bits(64, p, 3)

    # p = 2**(-84.13)
    p = 2**(-102.24)
    # after_correcting_k_bits(512, p, 3)
    est_after_correcting_k_bits(512, p, 3)
    # p = 2**(-80.95)
    p = 2**(-98.87)
    # after_correcting_k_bits(64, p, 3)
    est_after_correcting_k_bits(64, p, 3)

    # p = 2**(-102.7)
    # after_correcting_k_bits(512, p, 3)
    # est_after_correcting_k_bits(512, p, 3)
    
    p = 2**(-147.40)
    after_correcting_k_bits(256, p, 0)
    p = 2**(-173.50)
    after_correcting_k_bits(256, p, 0)
    p = 2**(-183.44)
    after_correcting_k_bits(256, p, 0)
