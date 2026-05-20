/* NIST AES256-CTR-DRBG using in-tree aes256ctr (no OpenSSL).
 * Optional USE_GETRANDOM: OS RNG for randombytes() until randombytes_init()
 * is called (KAT / deterministic tools seed the DRBG via randombytes_init). */
#define _GNU_SOURCE
#include <string.h>
#include "rng.h"
#include "aes256ctr.h"

#if defined(USE_GETRANDOM) && defined(__linux__)
#include <errno.h>
#include <sys/random.h>
#endif

static AES256_CTR_DRBG_struct DRBG_ctx;
static int randombytes_kat_mode;

static void aes256_ecb(const unsigned char *key, unsigned char *ctr, unsigned char *out)
{
  aes256ctr_ctx s;
  uint8_t block[AES256CTR_BLOCKBYTES];
  uint8_t nonce[12];

  /* aes256ctr_squeezeblocks writes 64 bytes; DRBG needs 16-byte AES output */
  memcpy(nonce, ctr, 12);
  aes256ctr_init(&s, key, nonce);
  aes256ctr_squeezeblocks(block, 1, &s);
  memcpy(out, block, 16);
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

  randombytes_kat_mode = 1;

  (void)security_strength;
  /* NIST AES-256-DRBG: 256-bit entropy minimum; tools pass 32 bytes, zero-pad to 48 */
  memset(seed_material, 0, 48);
  memcpy(seed_material, entropy_input, 32);
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
#if defined(USE_GETRANDOM) && defined(__linux__)
  if(!randombytes_kat_mode) {
    unsigned char *p = x;
    while(xlen > 0) {
      size_t chunk = xlen > (unsigned long long)1048576 ? (size_t)1048576 : (size_t)xlen;
      ssize_t ret = getrandom(p, chunk, 0);
      if(ret < 0) {
        if(errno == EINTR)
          continue;
        return RNG_BAD_OUTBUF;
      }
      if(ret == 0)
        return RNG_BAD_OUTBUF;
      p += ret;
      xlen -= (unsigned long long)ret;
    }
    return RNG_SUCCESS;
  }
#endif

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
