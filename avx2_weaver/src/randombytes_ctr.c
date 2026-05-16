/* NIST AES256-CTR-DRBG using in-tree aes256ctr (no OpenSSL). */
#include <string.h>
#include "rng.h"
#include "aes256ctr.h"

static AES256_CTR_DRBG_struct DRBG_ctx;

static void aes256_ecb(const unsigned char *key, unsigned char *ctr, unsigned char *out)
{
  aes256ctr_ctx s;
  uint8_t nonce[12];

  memcpy(nonce, ctr, 12);
  aes256ctr_init(&s, key, nonce);
  aes256ctr_squeezeblocks(out, 1, &s);
}

void AES256_CTR_DRBG_Update(unsigned char *provided_data,
                            unsigned char *Key,
                            unsigned char *V)
{
  unsigned char temp[48];
  unsigned int i, j;

  for(i = 0; i < 3; i++) {
    for(j = 15; j < 16; j--) {
      if(V[j] == 0xff)
        V[j] = 0x00;
      else {
        V[j]++;
        break;
      }
    }
    aes256_ecb(Key, V, temp + 16*i);
  }
  if(provided_data != NULL) {
    for(i = 0; i < 48; i++)
      temp[i] ^= provided_data[i];
  }
  memcpy(Key, temp, 32);
  memcpy(V, temp + 32, 16);
}

void randombytes_init(unsigned char *entropy_input,
                      unsigned char *personalization_string,
                      int security_strength)
{
  unsigned char seed_material[48];
  unsigned int i;

  (void)security_strength;
  memcpy(seed_material, entropy_input, 48);
  if(personalization_string) {
    for(i = 0; i < 48; i++)
      seed_material[i] ^= personalization_string[i];
  }
  memset(DRBG_ctx.Key, 0, 32);
  memset(DRBG_ctx.V, 0, 16);
  AES256_CTR_DRBG_Update(seed_material, DRBG_ctx.Key, DRBG_ctx.V);
  DRBG_ctx.reseed_counter = 1;
}

int randombytes(unsigned char *x, unsigned long long xlen)
{
  unsigned char block[16];
  unsigned long long i = 0;
  unsigned int j;

  while(xlen > 0) {
    for(j = 15; j < 16; j--) {
      if(DRBG_ctx.V[j] == 0xff)
        DRBG_ctx.V[j] = 0x00;
      else {
        DRBG_ctx.V[j]++;
        break;
      }
    }
    aes256_ecb(DRBG_ctx.Key, DRBG_ctx.V, block);
    if(xlen > 15) {
      memcpy(x + i, block, 16);
      i += 16;
      xlen -= 16;
    } else {
      memcpy(x + i, block, (size_t)xlen);
      xlen = 0;
    }
  }
  AES256_CTR_DRBG_Update(NULL, DRBG_ctx.Key, DRBG_ctx.V);
  DRBG_ctx.reseed_counter++;
  return RNG_SUCCESS;
}
