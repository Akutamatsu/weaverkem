#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "api.h"
#include "params.h"
#include "indcpa.h"
#include "poly.h"
#include "polyvec.h"
#include "cpucycles.h"
#include "speed_print.h"
#include "symmetric.h"
#include "rng.h"

//#define ALL_TESTS
#define NTESTS 10000

uint64_t t[NTESTS];
uint8_t seed[WEAVER_SYMBYTES] = {0};

int main()
{
  unsigned int i;
  unsigned char pk[CRYPTO_PUBLICKEYBYTES] = {0};
  unsigned char sk[CRYPTO_SECRETKEYBYTES] = {0};
  unsigned char ct[CRYPTO_CIPHERTEXTBYTES] = {0};
  unsigned char key[CRYPTO_BYTES] = {0};
  polyvec matrix[WEAVER_K];
  polyvec sp, pkpv;
  poly ap;
  unsigned int nonce = 0;
  uint8_t buf[2*WEAVER_SYMBYTES];
  uint8_t kr[2*WEAVER_SYMBYTES];

  printf("%s start..\n", CRYPTO_ALGNAME);
  
#ifdef ALL_TESTS
  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    gen_matrix(matrix, seed, 0);
  }
  print_results("gen_a: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    poly_getnoise_eta1(&ap, seed, 0);
  }
  print_results("poly_getnoise_eta1: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    poly_getnoise_eta2(&ap, seed, 0);
  }
  print_results("poly_getnoise_eta2: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    poly_ntt(&ap);
  }
  print_results("NTT: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    poly_invntt_tomont(&ap);
  }
  print_results("INVNTT: ", t, NTESTS);

  for(i=0;i<WEAVER_K;i++)
    poly_getnoise_eta2(sp.vec+i, seed, nonce++);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    polyvec_basemul_acc_montgomery(&ap, &matrix[0], &sp);
  }
  print_results("polyvec_basemul_acc_montgomery: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    poly_reduce(&ap);
  }
  print_results("poly_reduce: ", t, NTESTS);
#endif

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    crypto_kem_keypair(pk, sk);
  }
  print_results("keypair: ", t, NTESTS);

#ifdef ALL_TESTS
  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    randombytes(buf, WEAVER_SYMBYTES);
  }
  print_results("randombytes: ", t, NTESTS);
  
  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    hash_h(buf, pk, WEAVER_PUBLICKEYBYTES);
  }
  print_results("hash_h: ", t, NTESTS);

   for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    hash_g(kr, buf, 2*WEAVER_SYMBYTES);
  }
  print_results("hash_g: ", t, NTESTS);

   for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    unpack_pk(&pkpv, seed, pk);
  }
  print_results("unpack_pk: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    pack_ciphertext(ct, &sp, &ap);
  }
  print_results("pack_ciphertext: ", t, NTESTS);
#endif

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    crypto_kem_enc(ct, key, pk);
  }
  print_results("encaps: ", t, NTESTS);

  for(i=0;i<NTESTS;i++) {
    t[i] = cpucycles();
    crypto_kem_dec(key, ct, sk);
  }
  print_results("decaps: ", t, NTESTS);

  return 0;
}
