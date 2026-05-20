#ifndef MSGENC_H
#define MSGENC_H

#include <stdint.h>
#include "params.h"

#define ELL_BAR_BYTES        28 // ceil 220 bits = 28 bytes
#define ELL_BAR_NIBBLES      55 // ceil 220 bits = 55 NIBBLES
#define ELL_DDOT_BYTES       5  // ceil 33 bits = 5 bytes
#define ELL_DDOT_NIBBLES     9  // ceil 36 bits = 9 NIBBLES
#define LOW_ECC_NIBBLES      6  // (63,39,4) BCH code; ceil （4*6=24） = 6 NIBBLES 
#define LOW_CODEWORD_NIBBLES 15  // (ELL_DDOT_NIBBLES + LOW_ECC_NIBBLES) < 16 NIBBLES = 64 bits
#define LOW_CODEWORD_BYTES 8
#define D4_STEP_LEN 64



#define poly_frommsg KYBER_NAMESPACE(_poly_frommsg)
void poly_frommsg(poly *r, const uint8_t msg[KYBER_INDCPA_MSGBYTES]);
#define poly_tomsg KYBER_NAMESPACE(_poly_tomsg)
void poly_tomsg(uint8_t msg[KYBER_INDCPA_MSGBYTES], const poly *a);

#endif