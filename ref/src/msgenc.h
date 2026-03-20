#ifndef MSGENC_H
#define MSGENC_H

#include <stdint.h>
#include "params.h"

#if   (WEAVER_MODE == 1)
  #define ELL_BAR_BYTES      16 // 128 bits = 16 bytes
  #define ELL_DDOT_BYTES     0
  #define LOW_CODEWORD_BYTES 0

#elif (WEAVER_MODE == 3)
  #define ELL_BAR_BYTES      28 // 224 bits = 28 bytes
  #define ELL_DDOT_BYTES     4  // 32 bits = 4 bytes
  #define LOW_CODEWORD_BYTES 8  // 64 bits = 8 bytes

#elif (WEAVER_MODE == 5)
  #define ELL_BAR_BYTES      60 // 480 bits = 60 bytes
  #define ELL_DDOT_BYTES     4  // 32 bits = 4 bytes
  #define LOW_CODEWORD_BYTES 8  // 64 bits = 8 bytes

#else
  #error "WEAVER_MODE must be in {1,3,5}"
#endif

#define poly_frommsg KYBER_NAMESPACE(_poly_frommsg)
void poly_frommsg(poly *r, const uint8_t msg[KYBER_INDCPA_MSGBYTES]);
#define poly_tomsg KYBER_NAMESPACE(_poly_tomsg)
void poly_tomsg(uint8_t msg[KYBER_INDCPA_MSGBYTES], const poly *a);

#endif