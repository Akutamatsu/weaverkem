/* Internal BCH core. Include after one generated BCH parameter header.
 * Parameters, lengths and pointers are public; message/ECC contents are secret.
 * Field operations use fixed-round polynomial arithmetic. The reduction
 * polynomial is derived from PUBLIC entries of the generated log/exp tables,
 * rather than being hard-coded. Secret data never indexes those tables.
 */
#include <stdint.h>
#include <string.h>

static uint32_t ct_nz(uint32_t x)
{
    return 0u - ((x | (0u - x)) >> 31);
}

static uint32_t ct_eq(uint32_t a, uint32_t b)
{
    return ~ct_nz(a ^ b);
}

static uint32_t ct_lt(uint32_t a, uint32_t b)
{
    return 0u - (uint32_t)(((uint64_t)a - (uint64_t)b) >> 63);
}

/* All callers supply a value below 2*n. */
static unsigned int ct_mod(unsigned int x)
{
    return x - (bch.n & ~ct_lt(x, bch.n));
}

static unsigned int ct_mul(unsigned int a, unsigned int b)
{
    /* x * x^(m-1) mod the field polynomial. Both log-table addresses and
     * the resulting exp-table address depend only on public parameters.
     * This also works if the exp table's generator is not the element x. */
    unsigned int reduction = a_pow_tab[ct_mod(
        a_log_tab[1u << (bch.m - 1)] + a_log_tab[2])];
    unsigned int i, result = 0;
    for (i = 0; i < bch.m; ++i) {
        uint32_t high = 0u - ((a >> (bch.m - 1)) & 1u);
        result ^= a & (0u - (b & 1u));
        a = ((a << 1) & bch.n) ^ (reduction & high);
        b >>= 1;
    }
    return result;
}

static unsigned int ct_inverse(unsigned int a)
{
    /* a^(2^m-2), with exactly m-2 square/multiply steps and one square.
     * pd in BM is always nonzero; inverse(0) is harmlessly defined as 0. */
    unsigned int i, result = a;
    for (i = 1; i + 1 < bch.m; ++i)
        result = ct_mul(ct_mul(result, result), a);
    return ct_mul(result, result);
}

static uint32_t ct_encode_word(unsigned int nibble, unsigned int word)
{
    const volatile uint32_t *table = mod8_tab_half;
    uint32_t value = 0;
    unsigned int i;
    for (i = 0; i < 16; ++i)
        value |= table[i * BCH_ECC_WORDS + word] & ct_eq(i, nibble);
    return value;
}

static void ct_encode_nibble(uint32_t *state, unsigned int nibble)
{
    unsigned int i;
    nibble ^= state[0] >> 28;
    for (i = 0; i + 1 < BCH_ECC_WORDS; ++i)
        state[i] = ((state[i] << 4) | (state[i + 1] >> 28)) ^
                   ct_encode_word(nibble, i);
    state[BCH_ECC_WORDS - 1] = (state[BCH_ECC_WORDS - 1] << 4) ^
                              ct_encode_word(nibble, BCH_ECC_WORDS - 1);
}

/* unit_bits is public and is either 4 (nibbles) or 8 (bytes). */
static void ct_encode(const uint8_t *data, unsigned int units,
                      unsigned int unit_bits, uint8_t *ecc)
{
    uint32_t state[BCH_ECC_WORDS] = {0};
    unsigned int i;
    for (i = 0; i < units; ++i) {
        if (unit_bits == 8) {
            ct_encode_nibble(state, data[i] >> 4);
            ct_encode_nibble(state, data[i] & 15u);
        } else {
            unsigned int shift = (i & 1u) ? 0u : 4u;
            ct_encode_nibble(state, (data[i / 2] >> shift) & 15u);
        }
    }
    for (i = 0; i < BCH_ECC_BYTES; ++i)
        ecc[i] = (uint8_t)(state[i / 4] >> (24u - 8u * (i & 3u)));
}

static void ct_syndromes(const uint8_t *ecc, unsigned int *syn)
{
    unsigned int bit, j;
    memset(syn, 0, 2 * BCH_T * sizeof(*syn));
    for (bit = 0; bit < bch.ecc_bits; ++bit) {
        uint32_t mask = 0u - ((ecc[bit / 8] >> (7u - (bit & 7u))) & 1u);
        unsigned int exponent = bch.ecc_bits - 1 - bit;
        for (j = 0; j < bch.t; ++j) {
            /* This index depends only on public loop counters/parameters. */
            unsigned int power = (unsigned int)
                (((uint64_t)exponent * (2u * j + 1u)) % bch.n);
            syn[2 * j] ^= a_pow_tab[power] & mask;
        }
    }
    /* syn[k] = S_(k+1); in characteristic two, S_(2j) = S_j^2. */
    for (j = 0; j < bch.t; ++j)
        syn[2 * j + 1] = ct_mul(syn[j], syn[j]);
}

/* Same simplified binary BM recurrence as ref/src, with fixed-address
 * shift/add and masked state replacement. All coefficient storage is initialized.
 */
static void ct_locator(const unsigned int *syn, struct gf_poly *elp)
{
    struct gf_poly previous = {0};
    unsigned int saved[2 * BCH_T + 1];
    unsigned int scaled[2 * BCH_T + 1];
    unsigned int i, j, dst, pd = 1, d = syn[0];
    /* pp+1 is kept unsigned, avoiding signed arithmetic and negative shifts. */
    unsigned int pp_plus_one = 0;
    memset(elp, 0, sizeof(*elp));
    previous.c[0] = elp->c[0] = 1;

    for (i = 0; i < bch.t; ++i) {
        unsigned int shift = 2 * i + 1 - pp_plus_one;
        unsigned int ratio = ct_mul(d, ct_inverse(pd));
        uint32_t active = ct_nz(d);
        unsigned int candidate;
        uint32_t replace;

        memcpy(saved, elp->c, sizeof(saved));
        for (j = 0; j <= i; ++j) {
            uint32_t use = active & ct_nz(previous.c[j]) &
                          ~ct_lt(previous.deg, j);
            scaled[j] = ct_mul(ratio, previous.c[j]) & use;
        }
        for (dst = 0; dst <= 2 * bch.t; ++dst)
            for (j = 0; j <= i; ++j)
                elp->c[dst] ^= scaled[j] & ct_eq(dst, j + shift);

        candidate = previous.deg + (shift & active);
        replace = ct_lt(elp->deg, candidate);
        for (j = 0; j <= 2 * bch.t; ++j) {
            uint32_t copy = replace & ~ct_lt(i, j);
            previous.c[j] = (saved[j] & copy) | (previous.c[j] & ~copy);
        }
        previous.deg = (elp->deg & replace) | (previous.deg & ~replace);
        elp->deg = (candidate & replace) | (elp->deg & ~replace);
        pd = (d & replace) | (pd & ~replace);
        pp_plus_one = ((2 * i + 1) & replace) | (pp_plus_one & ~replace);

        if (i + 1 < bch.t) {
            d = syn[2 * i + 2];
            for (j = 1; j <= bch.t && j <= 2 * i + 2; ++j)
                d ^= ct_mul(elp->c[j], syn[2 * i + 2 - j]) &
                     ~ct_lt(elp->deg, j);
        }
    }
}

/* Reference Chien interval covers message bits only, not ECC bits. Each root
 * therefore corresponds to the current PUBLIC message position. Correct it
 * in place using a mask; no secret-indexed roots array or data write is needed.
 */
static int ct_correct(uint8_t *data, unsigned int bits, const struct gf_poly *elp)
{
    unsigned int current[BCH_T + 1], step[BCH_T + 1];
    unsigned int start = bch.n - bch.ecc_bits - bits;
    unsigned int j, bit, count = 0;
    for (j = 1; j <= bch.t; ++j) {
        unsigned int offset = (unsigned int)(((uint64_t)j * start) % bch.n);
        current[j] = ct_mul(elp->c[j], a_pow_tab[offset]);
        step[j] = a_pow_tab[j];
    }
    for (bit = 0; bit < bits; ++bit) {
        unsigned int value = elp->c[0];
        uint32_t root;
        for (j = 1; j <= bch.t; ++j) {
            current[j] = ct_mul(current[j], step[j]);
            value ^= current[j];
        }
        root = ct_eq(value, 0);
        count += root & 1u;
        data[bit / 8] ^= (uint8_t)((1u << (7u - (bit & 7u))) & root);
    }
    return (int)count;
}

static int ct_decode(uint8_t *data, unsigned int units,
                     unsigned int unit_bits, const uint8_t *recv_ecc)
{
    uint8_t ecc[BCH_ECC_BYTES];
    unsigned int syn[2 * BCH_T];
    struct gf_poly elp;
    unsigned int i;
    if (!data || !recv_ecc || units > (bch.n - bch.ecc_bits) / unit_bits)
        return -1;
    ct_encode(data, units, unit_bits, ecc);
    for (i = 0; i < BCH_ECC_BYTES; ++i)
        ecc[i] ^= recv_ecc[i];
    ct_syndromes(ecc, syn);
    ct_locator(syn, &elp);
    /* Like ref/src, return message-root count, without introducing a new
     * uncorrectable-error status or changing the caller's failure policy. */
    return ct_correct(data, units * unit_bits, &elp);
}
