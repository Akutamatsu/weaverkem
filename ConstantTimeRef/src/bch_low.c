# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <stdint.h>
# include "params.h"  // <--- 引入我们自己架构的参数
# include "bch.h"
#include "bch63.h"

#if BCH_ECC_WORDS != 1
#error "encode_bch_low: mod8 without table only implemented for BCH_ECC_WORDS==1"
#endif

/* One high-nibble (4 bits) of systematic BCH encode: same as precomputed mod8_tab_half[v]. */
static inline uint32_t bch_mod8_nibble_step(uint32_t G, unsigned int nib)
{
    uint32_t st = (nib & 15u) << 28;
    int s;
    for (s = 0; s < 4; s++)
        st = (st << 1) ^ (G & (0u - (st >> 31)));
    return st;
}

/* 0 or 0xffffffff: unsigned x != 0 */
static inline uint32_t bch_mask_u32_nz_u(unsigned int x)
{
    uint32_t u = (uint32_t)x;
    return (uint32_t)((((int32_t)u) | (-(int32_t)u)) >> 31);
}

/* all-1 iff a > b */
static inline uint32_t bch_mask_u32_gt_u(unsigned int a, unsigned int b)
{
    return ~(uint32_t)((int32_t)((int32_t)a - (int32_t)b - 1) >> 31);
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
    const uint32_t Gpoly = g[0];
    
    memset(ecc_buf,0,BCH_ECC_WORDS*sizeof(uint32_t));

    while (len--) 
    {
        unsigned int nib = ((ecc_buf[0] >> 28)^((*data)>>4)) & 15u;
        uint32_t w = bch_mod8_nibble_step(Gpoly, nib);
        for (i = 0; i < l; i++)
            ecc_buf[i] = ((ecc_buf[i] << 4)|(ecc_buf[i+1] >> 28))^w;
        ecc_buf[l] = (ecc_buf[l] << 4)^w;
        
        nib = ((ecc_buf[0] >> 28)^(*data)) & 15u;
        data++;
        w = bch_mod8_nibble_step(Gpoly, nib);
        for (i = 0; i < l; i++)
            ecc_buf[i] = ((ecc_buf[i] << 4)|(ecc_buf[i+1] >> 28))^w;
        ecc_buf[l] = (ecc_buf[l] << 4)^w;
    }
    
    store_ecc8(ecc,ecc_buf);    
}

//shorter and faster modulo function, only works when v < 2N.
static inline int mod_s(unsigned int v)
{
    unsigned int tmp;
    tmp = ~(uint32_t)((int32_t)((int32_t)v - (int32_t)bch.n) >> 31);

    return (int)(v - (bch.n & tmp));
}

//Galois field basic operations: multiply, divide, inverse, etc.
static inline unsigned int gf_mul(unsigned int a, unsigned int b)
{
    unsigned int tmp,mask;
    
    tmp=a_pow_tab[mod_s(a_log_tab[a]+a_log_tab[b])];
    mask = bch_mask_u32_nz_u(a | b);

    return  tmp&mask;
}

static inline unsigned int gf_sqr(unsigned int a)
{
    unsigned int tmp,mask;
    
    tmp=a_pow_tab[mod_s( 2*a_log_tab[a])];
    mask = bch_mask_u32_nz_u(a);

    return tmp&mask;
}

static inline int a_log(unsigned int x)
{
    return a_log_tab[x];
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
            mask_syn = 0u - ((poly >> i) & 1u);
            w=i+s;
            w2=w*2;
            for (j = 0; j < 2*t; j += 2)
            {
                syn_tmp=a_pow_tab[w];
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
    const unsigned int n = bch.n;
    unsigned int i, j, tmp, l, pd = 1, d = syn[0];
    
    struct gf_poly pelp ;
    struct gf_poly elp_copy ;
    int k, pp = -1;
    uint16_t mask_d,mask_pelp;
    unsigned int mask_tmp,mask_deg,tmp_c,mask_tmp2;
    
    memset(pelp.c, 0, (2*t+1)*sizeof(unsigned int));
    memset(elp->c, 0,(2*t+1)*sizeof(unsigned int));

    pelp.deg = 0;
    pelp.c[0] = 1;
    elp->deg = 0;
    elp->c[0] = 1;

    // use simplified binary Berlekamp-Massey algorithm 
    for (i = 0; i < t ; i++) 
    {
        mask_d = (uint16_t)(bch_mask_u32_nz_u(d) & 0xffffu);
        k = 2*i-pp;
            
        gf_poly_copy(&elp_copy, elp,i);
            // e[i+1](X) = e[i](X)+di*dp^-1*X^2(i-p)*e[p](X) 
        tmp = mod_s(a_log(d)+n-a_log(pd));

        for (j = 0; j <=i ; j++) 
        {
            mask_deg = ~(uint32_t)((int32_t)((int32_t)pelp.deg - (int32_t)j) >> 31);
            mask_pelp = (uint16_t)(bch_mask_u32_nz_u(pelp.c[j]) & 0xffffu);
            l = mod_s(tmp+a_log( pelp.c[j]));
            tmp_c=a_pow_tab[l];
            elp->c[j+k] ^= (tmp_c&mask_d&mask_pelp&mask_deg);
        }
        // compute l[i+1] = max(l[i]->c[l[p]+2*(i-p]) 
        tmp = pelp.deg+(k&mask_d);
        mask_tmp = ~(uint32_t)((int32_t)((int32_t)tmp - (int32_t)elp->deg - 1) >> 31);
        mask_tmp2 = ~mask_tmp;
        //memcpy cause 70 cycles gap
        {
            struct gf_poly pelp_alt, elp_copy_alt;
            unsigned int old_elp_deg = elp->deg;
            unsigned int jj;

            gf_poly_copy(&pelp_alt, &elp_copy, i);
            gf_poly_copy(&elp_copy_alt, elp, i);
            for (jj = 0; jj <= (unsigned int)i; jj++) {
                pelp.c[jj] = (pelp_alt.c[jj] & mask_tmp) | (pelp.c[jj] & mask_tmp2);
                elp_copy.c[jj] = (elp_copy.c[jj] & mask_tmp) | (elp_copy_alt.c[jj] & mask_tmp2);
            }
            pelp.deg = (pelp_alt.deg & mask_tmp) | (pelp.deg & mask_tmp2);
            elp_copy.deg = (elp_copy.deg & mask_tmp) | (elp_copy_alt.deg & mask_tmp2);
            elp->deg = (tmp & mask_tmp) | (old_elp_deg & mask_tmp2);
        }
        //update according to mask_tmp  
        pd = (d & mask_tmp)^(pd &mask_tmp2);
        pp = ((2*i) & mask_tmp)^(pp &mask_tmp2);
            
        // di+1 = S(2i+3)+elp[i+1].1*S(2i+2)+...+elp[i+1].lS(2i+3-l) 
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

    {
        uint32_t bad = bch_mask_u32_gt_u(elp->deg, t);
        return (int)(((uint32_t)elp->deg & ~bad) | bad);
    }
}

// build log-based representation of a polynomial
static void init_rep(const struct gf_poly *a, uint16_t *rep, uint16_t *syn_mask, unsigned int pow_start)
{
    int i,w,t=bch.t;
    w=pow_start;
    for (i = 1; i <= t; i++)
    {
        rep[i] =  mod_s(a_log(a->c[i])+ w);
        w=mod_s(w+pow_start);
        syn_mask[i] = (uint16_t)(bch_mask_u32_nz_u(a->c[i]) & 0xffffu);
    }
}

//exhaustive root search (Chien) implementation
static int chien_search(unsigned int len, struct gf_poly *p, unsigned int *roots)
{
    unsigned int i, j, syn, count = 0,n=bch.n,t=bch.t;
    unsigned int k = 8*len+bch.ecc_bits;
    unsigned int bound=n-bch.ecc_bits;
    uint16_t     syn_mask[BCH_T+1],syn_rep[BCH_T+1];
    unsigned int syn_tmp;
    unsigned int syn0=p->c[0];
    
    //use a log-based representation of polynomial 
    init_rep(p,syn_rep,syn_mask,n-k);
    
    for (i = n-k+1; i <= bound; i++) 
    {
        // compute elp(a^i) 
        syn = syn0;
        for (j = 1 ; j <= t; j++) 
        {
            syn_rep[j]=mod_s(syn_rep[j]+j);
            syn_tmp=a_pow_tab[syn_rep[j]];
            syn ^= (syn_tmp&syn_mask[j]);
        }
        {
            uint32_t mz = 0u - (uint32_t)(1u &
                (uint32_t)~(((uint32_t)syn | (uint32_t)(0u - (uint32_t)syn)) >> 31));
            roots[count] = (roots[count] & ~mz) | (((n - i) & mz));
            count += (unsigned int)(mz & 1u);
        }
    }
    
    return count;
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

    memset(errloc, 0, sizeof(errloc));

    // sanity check: make sure data length can be handled 
    if (8*len > (bch.n-bch.ecc_bits))
        return -1;
        
    //check data and ecc pointer
    if (!data || !recv_ecc)
        return -1;
        
    // compute received data ecc into an internal buffer 
    encode_bch_low(data, len, ecc_buf);
    // XOR received and calculated ecc 
    for (i = 0; i < bch.ecc_bytes; i++) 
    {
        ecc_buf[i] ^= recv_ecc[i];
    }
    
    //compute syndromes
    compute_syndromes(ecc_buf, syn);
    //compute error locator polynomial
    compute_error_locator_polynomial(syn,&elp);
    //find roots
    err=chien_search(len, &elp, errloc);

    //post process error location
    nbits = (len*8)+bch.ecc_bits;
    
    unsigned char mask_err;
    
    for (i = 0; i < bch.t; i++) 
    {
        mask_err = (unsigned char)((uint32_t)((int32_t)(i - err) >> 31) & 0xffu);
        errloc[i] = (nbits-1-errloc[i])&mask_err;
        errloc[i] = ((errloc[i] & ~7)|(7-(errloc[i] & 7)))&mask_err;
        data[errloc[i]/8] ^= ((1 << (errloc[i] % 8))&mask_err);
    }
    
    return err;
}