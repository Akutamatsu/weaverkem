#ifndef INVQ_H
#define INVQ_H

/* ====================================================================
 * WEAVER-Inv: Randomized Lifting (Inv_q) 声明
 *
 * 本头文件必须在 polyvec.h 之后 include，因为使用了 polyvec 类型。
 * 仅被 indcpa.c 和 poly_invq.c 使用。
 * ==================================================================== */

#include <stdint.h>
#include "params.h"
#include "polyvec.h"

#define polyvec_invq KYBER_NAMESPACE(_polyvec_invq)
void polyvec_invq(polyvec *v,
                    const uint8_t seed[KYBER_SYMBYTES],
                    uint8_t nonce);

#endif /* INVQ_H */
