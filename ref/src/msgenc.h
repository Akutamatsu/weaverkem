#ifndef MSGENC_H
#define MSGENC_H

#include <stdint.h>
#include "params.h"

#if   (WEAVER_MODE == 1)
  /* (255,215,5): 40 bits = 5 B ECC overhead */
  #define ELL_BAR_BYTES        16 // 128 bits = 16 bytes
  #define HIGH_ECC_BYTES       5
  #define HIGH_CODEWORD_BITS   WEAVER_POLYCUT_DIMENSION // 128 + 40
  #define HIGH_CODEWORD_BYTES  (HIGH_CODEWORD_BITS >> 3)
  #define ELL_DDOT_BYTES       0
  #define LOW_CODEWORD_BYTES   0

#elif (WEAVER_MODE == 3)
  #define ELL_BAR_BYTES        28 // ceil 220 bits = 28 bytes
  #define ELL_BAR_NIBBLES      55 // ceil 220 bits = 55 NIBBLES
  #define ELL_DDOT_BYTES       5  // ceil 33 bits = 5 bytes
  #define ELL_DDOT_NIBBLES     9  // ceil 36 bits = 9 NIBBLES
  #define LOW_ECC_NIBBLES      6  // (63,39,4) BCH code; ceil （4*6=24） = 6 NIBBLES 
  #define LOW_CODEWORD_NIBBLES 15  // (ELL_DDOT_NIBBLES + LOW_ECC_NIBBLES) < 16 NIBBLES = 64 bits
  #define LOW_CODEWORD_BYTES 8
  #define D4_STEP_LEN 64

#elif (WEAVER_MODE == 5)
  /* High: (511,*,6) shortened BCH — payload <= floor((511-54)/8)=57 bytes (matches bch511_456_6.h). */
  #define ELL_BAR_BYTES      57
  /* Low: (127,*,6) — payload <= floor((127-42)/8)=10 bytes; use 7 data + 6 ECC = 13 (64 - 57 = 7). */
  #define ELL_DDOT_BYTES     7
  #define LOW_ECC_BYTES      6  /* BCH_ECC_BYTES from bch127_56_6.h */
  #define LOW_CODEWORD_BYTES 13  /* ELL_DDOT_BYTES + LOW_ECC_BYTES */
  #define D4_STEP_LEN 128

#else
  #error "WEAVER_MODE must be in {1,3,5}"
#endif

#define poly_frommsg WEAVER_NAMESPACE(_poly_frommsg)
void poly_frommsg(poly *r, const uint8_t msg[WEAVER_INDCPA_MSGBYTES]);
#define poly_tomsg WEAVER_NAMESPACE(_poly_tomsg)
void poly_tomsg(uint8_t msg[WEAVER_INDCPA_MSGBYTES], const poly *a);

#define poly_compress WEAVER_NAMESPACE(_poly_compress)
void poly_compress(uint8_t r[WEAVER_POLYCOMPRESSEDBYTES], const poly *a);
#define poly_decompress WEAVER_NAMESPACE(_poly_decompress)
void poly_decompress(poly *r, const uint8_t a[WEAVER_POLYCOMPRESSEDBYTES]);

#endif