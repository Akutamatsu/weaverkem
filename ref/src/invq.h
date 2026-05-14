#ifndef INVQ_H
#define INVQ_H

#include <stdint.h>
#include "params.h"
#include "polyvec.h"

#define polyvec_invq KYBER_NAMESPACE(_polyvec_invq)
void polyvec_invq(polyvec *v,
                    const uint8_t seed[KYBER_SYMBYTES],
                    uint8_t nonce);

#endif /* INVQ_H */
