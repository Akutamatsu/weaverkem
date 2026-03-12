#include <string.h>
#include "api.h"
#include "indcpa.h"
#include "params.h"
#include "rng.h"
#include "fips202.h"
#include "verify.h"

/*************************************************
* Name:        crypto_kem_keypair
*
* Description: Generates public and private key
*              for CCA secure NewHope key encapsulation
*              mechanism
*
* Arguments:   - unsigned char *pk: pointer to output public key (an already allocated array of CRYPTO_PUBLICKEYBYTES bytes)
*              - unsigned char *sk: pointer to output private key (an already allocated array of CRYPTO_SECRETKEYBYTES bytes)
*
* Returns 0 (success)
**************************************************/
int crypto_kem_keypair(unsigned char *pk, unsigned char *sk)
{
  indcpa_keypair(pk, sk);                                                        /* First put the actual secret key into sk */

  return 0;
}

/*************************************************
* Name:        crypto_kem_enc
*
* Description: Generates cipher text and shared
*              secret for given public key
*
* Arguments:   - unsigned char *ct:       pointer to output cipher text (an already allocated array of CRYPTO_CIPHERTEXTBYTES bytes)
*              - unsigned char *ss:       pointer to output shared secret (an already allocated array of CRYPTO_BYTES bytes)
*              - const unsigned char *pk: pointer to input public key (an already allocated array of CRYPTO_PUBLICKEYBYTES bytes)
*
* Returns 0 (success)
**************************************************/
int crypto_kem_enc(unsigned char *ct, unsigned char *ss, const unsigned char *pk)
{
  unsigned char buf[KYBER_INDCPA_MSGBYTES + KYBER_SYMBYTES];

  randombytes(buf,KYBER_SYMBYTES);

  shake256(buf,KYBER_INDCPA_MSGBYTES + KYBER_SYMBYTES,buf,KYBER_SYMBYTES);                         /* Don't release system RNG output */

  indcpa_enc(ct, buf, pk, buf+KYBER_INDCPA_MSGBYTES);                                 /* coins are in buf+KYBER_SYMBYTES */

  memcpy(ss, buf, KYBER_INDCPA_MSGBYTES);
  //shake256(ss, KYBER_SYMBYTES, buf, KYBER_SYMBYTES);                         /* hash pre-k to ss */
  return 0;
}


/*************************************************
* Name:        crypto_kem_dec
*
* Description: Generates shared secret for given
*              cipher text and private key
*
* Arguments:   - unsigned char *ss:       pointer to output shared secret (an already allocated array of CRYPTO_BYTES bytes)
*              - const unsigned char *ct: pointer to input cipher text (an already allocated array of CRYPTO_CIPHERTEXTBYTES bytes)
*              - const unsigned char *sk: pointer to input private key (an already allocated array of CRYPTO_SECRETKEYBYTES bytes)
*
* Returns 0 (success)
**************************************************/
int crypto_kem_dec(unsigned char *ss, const unsigned char *ct, const unsigned char *sk)
{
  indcpa_dec(ss, ct, sk);

  //shake256(ss, KYBER_SYMBYTES, ss, KYBER_SYMBYTES);                          /* hash pre-k to ss */

  return 0;
}
