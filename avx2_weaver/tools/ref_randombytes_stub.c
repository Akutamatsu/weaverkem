/* Stub RNG for ref-only tools (no OpenSSL). */
#include <stdint.h>

void randombytes_init(unsigned char *entropy_input,
                      unsigned char *personalization_string,
                      int security_strength)
{
  (void)entropy_input;
  (void)personalization_string;
  (void)security_strength;
}

int randombytes(unsigned char *x, unsigned long long xlen)
{
  static uint32_t s = 1;
  unsigned long long i;
  for(i = 0; i < xlen; i++) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    x[i] = (unsigned char)s;
  }
  return 0;
}
