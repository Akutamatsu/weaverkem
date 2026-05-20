# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <stdint.h>
# include "params.h"  
# include "bch.h"

#if WEAVER_MODE == 3 || WEAVER_MODE == 5
     
    #if WEAVER_MODE == 3
        // bch(255, 224, 3) 
        #include "bch63_33_4.h"
    #elif WEAVER_MODE == 5
        // bch(511, 474, 4) 
        #include "bch127_92_5.h"
    #else
        #error "Invalid WEAVER_MODE for BCH configuration"
    #endif

/* 与 bch_high.c 一致：掩码、无秘密下标查表、mod8 行扫描（encode）；syndrome/BM/Chien/decode 同构 */
static inline uint32_t bch_mask_u32_nz_u(unsigned int x)
{
    uint32_t u = (uint32_t)x;
    return (uint32_t)((((int32_t)u) | (-(int32_t)u)) >> 31);
}

static inline uint32_t mod8_tab_word(unsigned int nib, unsigned int word_idx)
{
    uint32_t acc = 0;
    unsigned int v;
    for (v = 0; v < 16; v++) {
        uint32_t m = 0u - (uint32_t)(v == (nib & 15u));
        acc |= (mod8_tab_half[(unsigned int)BCH_ECC_WORDS * v + word_idx] & m);
    }
    return acc;
}

// convert 32-bit ecc words to ecc bytes
static void store_ecc8(uint8_t *dst, const uint32_t *src)
{
    uint8_t pad[4];
    unsigned int i, nwords = BCH_ECC_WORDS-1;

    for (i = 0; i < nwords; i++) {
        *dst++ = (src[i] >> 24);
        *dst++ = (src[i] >> 16) & 0xff;
        *dst++ = (src[i] >>  8) & 0xff;
        *dst++ = (src[i] >>  0) & 0xff;
    }
    pad[0] = (src[nwords] >> 24);
    pad[1] = (src[nwords] >> 16) & 0xff;
    pad[2] = (src[nwords] >>  8) & 0xff;
    pad[3] = (src[nwords] >>  0) & 0xff;
    memcpy(dst, pad, BCH_ECC_BYTES-4*nwords);
}

// bch encode
void encode_bch_low(const unsigned char *data, unsigned int len, uint8_t *ecc)
{
    int i;
    const int l = BCH_ECC_WORDS-1;
    uint32_t ecc_buf[BCH_ECC_WORDS];
    unsigned int nib;
    
    memset(ecc_buf,0,BCH_ECC_WORDS*sizeof(uint32_t));

    while (len--) 
    {
        nib = ((ecc_buf[0] >> 28)^((*data)>>4)) & 0x0fu;
        for (i = 0; i < l; i++)
            ecc_buf[i] = ((ecc_buf[i] << 4)|(ecc_buf[i+1] >> 28))^mod8_tab_word(nib, (unsigned)i);
        ecc_buf[l] = (ecc_buf[l] << 4)^mod8_tab_word(nib, (unsigned)l);
        
        nib = ((ecc_buf[0] >> 28)^(*data)) & 0x0fu;
        data++;
        for (i = 0; i < l; i++)
            ecc_buf[i] = ((ecc_buf[i] << 4)|(ecc_buf[i+1] >> 28))^mod8_tab_word(nib, (unsigned)i);
        ecc_buf[l] = (ecc_buf[l] << 4)^mod8_tab_word(nib, (unsigned)l);
    }
    
    store_ecc8(ecc,ecc_buf);    
}

// shorter and faster modulo function, only works when v < 2N.
static inline int mod_s(unsigned int v)
{
    uint32_t tmp = ~(uint32_t)((int32_t)((int32_t)v - (int32_t)bch.n) >> 31);
    return (int)(v - (bch.n & tmp));
}

static inline unsigned int a_pow_ct(unsigned int e)
{
    unsigned int res = 0;
    unsigned int i;
    unsigned int em = (unsigned int)mod_s(e);
    for (i = 0; i < bch.n; i++) {
        uint32_t mask = 0u - (uint32_t)(i == em);
        res |= ((uint32_t)a_pow_tab[i] & mask);
    }
    return res;
}

static inline unsigned int a_log_ct(unsigned int x)
{
    unsigned int res = 0;
    unsigned int i;
    for (i = 0; i <= bch.n; i++) {
        uint32_t mask = 0u - (uint32_t)(i == x);
        res |= ((uint32_t)a_log_tab[i] & mask);
    }
    return res;
}

//Galois field basic operations: multiply, divide, inverse, etc.
static inline unsigned int gf_mul(unsigned int a, unsigned int b)
{
    uint32_t mask = bch_mask_u32_nz_u(a) & bch_mask_u32_nz_u(b);
    unsigned int lg = (unsigned int)mod_s((unsigned int)a_log_ct(a) + (unsigned int)a_log_ct(b));
    return a_pow_ct(lg) & mask;
}

static inline unsigned int gf_sqr(unsigned int a)
{
    uint32_t mask = bch_mask_u32_nz_u(a);
    unsigned int lg = (unsigned int)mod_s(2u * (unsigned int)a_log_ct(a));
    return a_pow_ct(lg) & mask;
}

static inline int a_log(unsigned int x)
{
    return (int)a_log_ct(x);
}

// compute 2t syndromes of ecc polynomial, i.e. ecc(a^j) for j=1..2t
static void compute_syndromes(uint8_t *ecc, unsigned int *syn)
{
    int i, j, s;
    unsigned int m;
    uint32_t poly,mask_syn,syn_tmp;
    const int t = bch.t;
    unsigned int w,w2;

    s = bch.ecc_bits;
    //make sure extra bits in last ecc byte are cleared 
    m = s & 7;
    if (m)
        ecc[s/8] &= ~((1u << (8-m))-1);
        
    memset(syn, 0, 2*t*sizeof(*syn));

    //compute v(a^j) for j=1 .. 2t-1 
    do {
        poly = *ecc++;
        s -= 8;
        
        for(i=7;i>=0 && i+s>=0;i--)
        {
            mask_syn = 0u - (uint32_t)((poly >> i) & 1u);
            w=i+s;
            w2=w*2;
            for (j = 0; j < 2*t; j += 2)
            {
                syn_tmp = a_pow_ct(w);
                syn[j] ^= (syn_tmp&mask_syn);
                w=mod_s(w+w2);
            }
        }
    } while (s > 0);

    // v(a^(2j)) = v(a^j)^2 
    for (j = 0; j < t; j++)
        syn[2*j+1] = gf_sqr(syn[j]);
}

static void gf_poly_copy(struct gf_poly *dst, struct gf_poly *src, int t)
{
    dst->deg=src->deg;
    memcpy(dst->c, src->c, (t+1)*sizeof(unsigned int));
}

static int compute_error_locator_polynomial(const unsigned int *syn, struct gf_poly *elp)
{
    const unsigned int t = bch.t;
    unsigned int i, j, tmp, pd = 1, d = syn[0];
    unsigned int inv_pd_di;
    
    struct gf_poly pelp ;
    struct gf_poly elp_copy ;
    int k, pp = -1;
    uint16_t mask_d;
    unsigned int mask_tmp, mask_tmp2;
    
    memset(pelp.c, 0, (2*t+1)*sizeof(unsigned int));
    memset(elp->c, 0,(2*t+1)*sizeof(unsigned int));

    pelp.deg = 0;
    pelp.c[0] = 1;
    elp->deg = 0;
    elp->c[0] = 1;

    /* simplified binary Berlekamp-Massey */
    for (i = 0; i < t ; i++) 
    {
        mask_d = (uint16_t)(bch_mask_u32_nz_u(d) & 0xffffu);
        k = 2*(int)i-pp;
            
        gf_poly_copy(&elp_copy, elp,(int)i);
        tmp = (unsigned int)mod_s((unsigned int)a_log_ct(d)+bch.n-(unsigned int)a_log_ct(pd));
        inv_pd_di = a_pow_ct(tmp);

        /* Constant-time shift-add (同 bch_high.c) */
        for (j = 0; j <= 2*t; j++) {
            unsigned int shifted_term = 0;
            unsigned int m;
            for (m = 0; m <= 2*t; m++) {
                int32_t diff = (int32_t)j - (int32_t)m - (int32_t)k;
                uint32_t mask_match = (uint32_t)(0u - (uint32_t)(diff == 0));
                uint32_t m_le_i = ~(uint32_t)((int32_t)((int32_t)m - (int32_t)i - 1) >> 31);
                uint32_t m_le_deg = ~(uint32_t)((int32_t)((int32_t)m - (int32_t)pelp.deg - 1) >> 31);
                shifted_term ^= (gf_mul(inv_pd_di, pelp.c[m]) & mask_match & m_le_i & m_le_deg);
            }
            elp->c[j] ^= (shifted_term & (unsigned int)mask_d);
        }
        tmp = pelp.deg + (((unsigned int)k & (unsigned int)mask_d));
        mask_tmp = ~(uint32_t)((int32_t)((int32_t)tmp - (int32_t)elp->deg - 1) >> 31);
        mask_tmp2 = ~mask_tmp;
        {
            struct gf_poly pelp_alt, elp_copy_alt;
            unsigned int old_elp_deg = elp->deg;
            unsigned int jj;

            gf_poly_copy(&pelp_alt, &elp_copy,(int)i);
            gf_poly_copy(&elp_copy_alt, elp,(int)i);
            for (jj = 0; jj <= i; jj++) {
                pelp.c[jj] = (pelp_alt.c[jj] & mask_tmp) | (pelp.c[jj] & mask_tmp2);
                elp_copy.c[jj] = (elp_copy.c[jj] & mask_tmp) | (elp_copy_alt.c[jj] & mask_tmp2);
            }
            pelp.deg = (pelp_alt.deg & mask_tmp) | (pelp.deg & mask_tmp2);
            elp_copy.deg = (elp_copy.deg & mask_tmp) | (elp_copy_alt.deg & mask_tmp2);
            elp->deg = (tmp & mask_tmp) | (old_elp_deg & mask_tmp2);
        }
        pd = (d & mask_tmp)^(pd &mask_tmp2);
        pp = ((2*i) & mask_tmp)^(pp &mask_tmp2);
            
        {
            unsigned int mask_j,tmp_d;
            if (i < t-1) 
            {
                d = syn[2*i+2];
            
                for (j = 1; j <=i+1 ; j++)
                {
                    mask_j = ~(uint32_t)((int32_t)((int32_t)elp->deg - (int32_t)j) >> 31);
                    tmp_d=gf_mul( elp->c[j], syn[2*i+2-j]);
                    d ^= (tmp_d&mask_j);
                }
            }
        }
    }

    {
        unsigned int deg = elp->deg;
        unsigned int m = 0u - (deg > t);
        return (int)((~0u & m) | (deg & ~m));
    }
}

// build log-based representation of a polynomial
static void init_rep(const struct gf_poly *a, uint16_t *rep, uint16_t *syn_mask, unsigned int pow_start)
{
    int i,w,t=bch.t;
    w=pow_start;
    for (i = 1; i <= t; i++)
    {
        rep[i] = (uint16_t)mod_s((unsigned int)((int)a_log(a->c[i]) + w));
        w = (int)mod_s((unsigned int)w + pow_start);
        syn_mask[i]=(uint16_t)(bch_mask_u32_nz_u(a->c[i]) & 0xffffu);
    }
}

//exhaustive root search (Chien) implementation
static int chien_search(unsigned int len, struct gf_poly *p, unsigned int *roots)
{
    unsigned int i, j, syn, count = 0,n=bch.n,t=bch.t;
    unsigned int k = 8*len+bch.ecc_bits;
    unsigned int bound=n-bch.ecc_bits;
    uint16_t     syn_mask[BCH_T+1],syn_rep[BCH_T+1];
    unsigned int current_val[BCH_T+1];
    unsigned int step[BCH_T+1];
    unsigned int syn0=p->c[0];
    
    init_rep(p,syn_rep,syn_mask,n-k);

    for (j = 1; j <= t; j++) {
        current_val[j] = a_pow_ct(syn_rep[j]);
        step[j] = a_pow_ct(j);
    }
    
    for (i = n-k+1; i <= bound; i++) 
    {
        syn = syn0;
        for (j = 1 ; j <= t; j++) 
        {
            current_val[j] = gf_mul(current_val[j], step[j]);
            syn ^= (current_val[j] & (unsigned int)syn_mask[j]);
        }
        {
            uint32_t mz = 0u - (uint32_t)(1u &
                (uint32_t)~(((uint32_t)syn | (uint32_t)(0u - (uint32_t)syn)) >> 31));
            for (unsigned int r_idx = 0; r_idx < BCH_T; r_idx++) {
                uint32_t mask_fill = bch_mask_u32_nz_u((uint32_t)(r_idx == count)) & mz;
                roots[r_idx] = (roots[r_idx] & ~mask_fill) | (((n - i) & mask_fill));
            }
            count += (unsigned int)(mz & 1u);
        }
    }
    
    return (int)count;
}

/**
 * decode_bch - decode received codeword and find bit error locations
 * @bch:      BCH control structure
 * @data:     received data, ignored if @calc_ecc is provided
 * @len:      data length in bytes, must always be provided
 * @recv_ecc: received ecc, if NULL then assume it was XORed in @calc_ecc
 *
 * Returns:
 * The number of errors found, or -22 if decoding failed, or -1 if
 * invalid parameters were provided
 */
int decode_bch_low(uint8_t *data, unsigned int len, const uint8_t *recv_ecc)
{
    unsigned int nbits;
    int i, err;
    uint8_t ecc_buf[BCH_ECC_BYTES];
    struct gf_poly elp;
    unsigned int syn[2*BCH_T+1];
    unsigned int errloc[BCH_T];

    if (8*len > (bch.n-bch.ecc_bits)) {
        printf("bch超出容量");
        return -1;
    }
        
    if (!data || !recv_ecc)
        return -1;

    memset(errloc, 0, sizeof(errloc));
        
    encode_bch_low(data, len, ecc_buf);
    for (i = 0; i < bch.ecc_bytes; i++) 
    {
        ecc_buf[i] ^= recv_ecc[i];
    }
    
    compute_syndromes(ecc_buf, syn);
    compute_error_locator_polynomial(syn,&elp);
    err=chien_search(len, &elp, errloc);

    nbits = (len*8)+bch.ecc_bits;

    for (i = 0; i < (int)len; i++) {
        unsigned int byte_xor_mask = 0;
        int r;
        for (r = 0; r < (int)bch.t; r++) {
            uint32_t mask_err_r = (uint32_t)((int32_t)(r - err) >> 31);
            unsigned int bit_pos = nbits - 1 - errloc[r];
            bit_pos = (bit_pos & ~7u) | (7u - (bit_pos & 7u));
            uint32_t mask_this_byte = bch_mask_u32_nz_u((uint32_t)(i == (int)(bit_pos >> 3)));
            uint32_t bit_mask = 0;
            int b;
            for (b = 0; b < 8; b++) {
                bit_mask |= ((1u << b) & bch_mask_u32_nz_u((uint32_t)(b == (int)(bit_pos & 7))));
            }
            byte_xor_mask ^= (bit_mask & mask_this_byte & mask_err_r);
        }
        data[i] ^= (uint8_t)byte_xor_mask;
    }
    
    return err;
}

#if WEAVER_MODE == 3
void encode_bch_low_nibbles(const unsigned char *data, unsigned int nibbles, uint8_t *ecc)
{
    int i;
    const int l = BCH_ECC_WORDS-1;
    uint32_t ecc_buf[BCH_ECC_WORDS];
    unsigned int nib;
    
    memset(ecc_buf,0,BCH_ECC_WORDS*sizeof(uint32_t));

    while (nibbles >= 2) 
    {
        nib = ((ecc_buf[0] >> 28)^((*data)>>4)) & 0x0fu;
        for (i = 0; i < l; i++)
            ecc_buf[i] = ((ecc_buf[i] << 4)|(ecc_buf[i+1] >> 28))^mod8_tab_word(nib, (unsigned)i);
        ecc_buf[l] = (ecc_buf[l] << 4)^mod8_tab_word(nib, (unsigned)l);
        
        nib = ((ecc_buf[0] >> 28)^(*data)) & 0x0fu;
        data++;
        for (i = 0; i < l; i++)
            ecc_buf[i] = ((ecc_buf[i] << 4)|(ecc_buf[i+1] >> 28))^mod8_tab_word(nib, (unsigned)i);
        ecc_buf[l] = (ecc_buf[l] << 4)^mod8_tab_word(nib, (unsigned)l);
        
        nibbles -= 2;
    }
    
    if (nibbles == 1) 
    {
        nib = ((ecc_buf[0] >> 28)^((*data)>>4)) & 0x0fu;
        for (i = 0; i < l; i++)
            ecc_buf[i] = ((ecc_buf[i] << 4)|(ecc_buf[i+1] >> 28))^mod8_tab_word(nib, (unsigned)i);
        ecc_buf[l] = (ecc_buf[l] << 4)^mod8_tab_word(nib, (unsigned)l);
    }
    
    store_ecc8(ecc,ecc_buf);    
}

static int chien_search_nibbles(unsigned int nibbles, struct gf_poly *p, unsigned int *roots)
{
    unsigned int i, j, syn, count = 0, n=bch.n, t=bch.t;
    unsigned int k = 4*nibbles + bch.ecc_bits; 
    unsigned int bound = n - bch.ecc_bits;
    uint16_t     syn_mask[BCH_T+1], syn_rep[BCH_T+1];
    unsigned int current_val[BCH_T+1];
    unsigned int step[BCH_T+1];
    unsigned int syn0=p->c[0];
    
    init_rep(p,syn_rep,syn_mask,n-k);

    for (j = 1; j <= t; j++) {
        current_val[j] = a_pow_ct(syn_rep[j]);
        step[j] = a_pow_ct(j);
    }
    
    for (i = n-k+1; i <= bound; i++) 
    {
        syn = syn0;
        for (j = 1 ; j <= t; j++) 
        {
            current_val[j] = gf_mul(current_val[j], step[j]);
            syn ^= (current_val[j] & (unsigned int)syn_mask[j]);
        }
        {
            uint32_t mz = 0u - (uint32_t)(1u &
                (uint32_t)~(((uint32_t)syn | (uint32_t)(0u - (uint32_t)syn)) >> 31));
            for (unsigned int r_idx = 0; r_idx < BCH_T; r_idx++) {
                uint32_t mask_fill = bch_mask_u32_nz_u((uint32_t)(r_idx == count)) & mz;
                roots[r_idx] = (roots[r_idx] & ~mask_fill) | (((n - i) & mask_fill));
            }
            count += (unsigned int)(mz & 1u);
        }
    }
    
    return (int)count;
}

int decode_bch_low_nibbles(uint8_t *data, unsigned int nibbles, const uint8_t *recv_ecc)
{
    unsigned int nbits;
    int i, err;
    uint8_t ecc_buf[BCH_ECC_BYTES];
    struct gf_poly elp;
    unsigned int syn[2*BCH_T+1];
    unsigned int errloc[BCH_T];
    unsigned int codeword_bytes;

    if (4*nibbles > (bch.n-bch.ecc_bits))
        return -1;
        
    if (!data || !recv_ecc)
        return -1;

    memset(errloc, 0, sizeof(errloc));
        
    encode_bch_low_nibbles(data, nibbles, ecc_buf);
    
    for (i = 0; i < bch.ecc_bytes; i++) 
    {
        ecc_buf[i] ^= recv_ecc[i];
    }
    
    compute_syndromes(ecc_buf, syn);
    compute_error_locator_polynomial(syn,&elp);
    
    err = chien_search_nibbles(nibbles, &elp, errloc);

    nbits = (nibbles*4) + bch.ecc_bits;
    codeword_bytes = (nbits + 7u) / 8u;

    for (i = 0; i < (int)codeword_bytes; i++) {
        unsigned int byte_xor_mask = 0;
        int r;
        for (r = 0; r < (int)bch.t; r++) {
            uint32_t mask_err_r = (uint32_t)((int32_t)(r - err) >> 31);
            unsigned int bit_pos = nbits - 1 - errloc[r];
            bit_pos = (bit_pos & ~7u) | (7u - (bit_pos & 7u));
            uint32_t mask_this_byte = bch_mask_u32_nz_u((uint32_t)(i == (int)(bit_pos >> 3)));
            uint32_t bit_mask = 0;
            int b;
            for (b = 0; b < 8; b++) {
                bit_mask |= ((1u << b) & bch_mask_u32_nz_u((uint32_t)(b == (int)(bit_pos & 7))));
            }
            byte_xor_mask ^= (bit_mask & mask_this_byte & mask_err_r);
        }
        data[i] ^= (uint8_t)byte_xor_mask;
    }
    
    return err;
}
#endif

#endif