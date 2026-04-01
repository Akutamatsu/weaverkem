/*
The software is provided by the Institute of Commercial Cryptography Standards
(ICCS), and is used for algorithm submissions in the Next-generation Commercial
Cryptographic Algorithms Program (NGCC).

ICCS doesn't represent or warrant that the operation of the software will be
uninterrupted or error-free in all cases. ICCS will take no responsibility for
the use of the software or the results thereof, if the software is used for any
other purposes.

WeaverKEM-512 interface implementation.
Security level: 512-bit classical / 256-bit quantum (WEAVER_MODE=5).
*/

#define WEAVER_MODE 5

#include "KEM_WeaverKEM-512.h"
#include "drng.h"
#include "params.h"
#include "api.h"
#include <string.h>
#include <stdint.h>

extern DRNG_ctx drng_algorithm;

int randombytes(unsigned char *x, unsigned long long xlen)
{
    return get_random_number(&drng_algorithm, x, xlen * 8);
}

unsigned long long kem_get_pk_len_bytes() { return (unsigned long long)KYBER_PUBLICKEYBYTES; }
unsigned long long kem_get_sk_len_bytes() { return (unsigned long long)KYBER_SECRETKEYBYTES; }
unsigned long long kem_get_ss_len_bytes() { return (unsigned long long)KYBER_SSBYTES; }
unsigned long long kem_get_ct_len_bytes() { return (unsigned long long)KYBER_CIPHERTEXTBYTES; }

int kem_keygen(
    unsigned char *pk, unsigned long long *pk_len_bytes,
    unsigned char *sk, unsigned long long *sk_len_bytes)
{
    int ret = crypto_kem_keypair(pk, sk);
    if (ret != 0) return ret;
    *pk_len_bytes = (unsigned long long)KYBER_PUBLICKEYBYTES;
    *sk_len_bytes = (unsigned long long)KYBER_SECRETKEYBYTES;
    return 0;
}

int kem_enc(
    unsigned char *pk, unsigned long long pk_len_bytes,
    unsigned char *ss, unsigned long long *ss_len_bytes,
    unsigned char *ct, unsigned long long *ct_len_bytes)
{
    (void)pk_len_bytes;
    int ret = crypto_kem_enc(ct, ss, pk);
    if (ret != 0) return ret;
    *ss_len_bytes = (unsigned long long)KYBER_SSBYTES;
    *ct_len_bytes = (unsigned long long)KYBER_CIPHERTEXTBYTES;
    return 0;
}

int kem_dec(
    unsigned char *sk, unsigned long long sk_len_bytes,
    unsigned char *ct, unsigned long long ct_len_bytes,
    unsigned char *ss, unsigned long long *ss_len_bytes)
{
    (void)sk_len_bytes;
    (void)ct_len_bytes;
    int ret = crypto_kem_dec(ss, ct, sk);
    if (ret != 0) return -1;
    *ss_len_bytes = (unsigned long long)KYBER_SSBYTES;
    return 0;
}
