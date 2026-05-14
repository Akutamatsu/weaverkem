#ifndef MSGENC_H
#define MSGENC_H

#include <stdint.h>
#include "params.h"

#if   (WEAVER_MODE == 1)
  #define ELL_BAR_BYTES      16 // 128 bits = 16 bytes  
  #define ELL_DDOT_BYTES     0
  #define LOW_CODEWORD_BYTES 0

// #elif (WEAVER_MODE == 3)
//   #define ELL_BAR_BYTES      28 // ceil 223 bits = 28 bytes l1
//   #define ELL_DDOT_BYTES     5  // ceil 33 bits = 5 bytes   l2
//   #define LOW_ECC_BYTES      3  // (63,33,4) BCH code; ceil （4*6=24） = 3 bytes  
//   #define LOW_CODEWORD_BYTES 8  // (ELL_DDOT_BYTES + LOW_ECC_BYTES) <= 8 bytes = 64 bits l3

#elif (WEAVER_MODE == 3)
  #define ELL_BAR_NIBBLES      55 // ceil 220 bits = 55 NIBBLES l1
  #define ELL_DDOT_NIBBLES     9  // ceil 36 bits = 9 NIBBLES   l2
  #define LOW_ECC_NIBBLES      6  // (63,33,4) BCH code; ceil （4*6=24） = 6 NIBBLES 
  #define LOW_CODEWORD_NIBBLES 15  // (ELL_DDOT_NIBBLES + LOW_ECC_NIBBLES) <= 16 NIBBLES = 64 bits l3

#elif (WEAVER_MODE == 5)
  #define ELL_BAR_BYTES      58 // 464 bits = 58 bytes
  #define ELL_DDOT_BYTES     6  // 48 bits = 6 bytes
  #define LOW_ECC_BYTES      5  // (127,92,5) BCH code; ceil(5*7 bits) = 5 bytes
  #define LOW_CODEWORD_BYTES 11  // (ELL_DDOT_BYTES + LOW_ECC_BYTES) <= 16 bytes = 128 bits

#else
  #error "WEAVER_MODE must be in {1,3,5}"
#endif

#define poly_frommsg KYBER_NAMESPACE(_poly_frommsg)
void poly_frommsg(poly *r, const uint8_t msg[KYBER_INDCPA_MSGBYTES]);
#define poly_tomsg KYBER_NAMESPACE(_poly_tomsg)
void poly_tomsg(uint8_t msg[KYBER_INDCPA_MSGBYTES], const poly *a);

#endif