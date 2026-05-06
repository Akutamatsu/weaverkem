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

/* Inv_q 桶查找表类型 */
typedef struct {
  uint32_t bucket_lo[KYBER_Q];       /* 桶 i 的左边界 */
  uint8_t  bucket_size[KYBER_Q];      /* 桶 i 的大小   */
  uint16_t large_bucket_count;        /* 大桶的个数    */
  uint16_t small_size;               /* 小桶大小      */
  uint16_t large_size;               /* 大桶大小      */
  int      d;                         /* 压缩精度      */
} invq_table_t;

/* 全局公钥提升表（定义在 poly_invq.c，此处 extern 声明） */
extern invq_table_t invq_pk_table;

#define invq_global_init KYBER_NAMESPACE(_invq_global_init)
void invq_global_init(void);

#define poly_invq KYBER_NAMESPACE(_poly_invq)
void poly_invq(poly *r, const uint8_t *randbuf, const invq_table_t *tbl);

#define polyvec_invq KYBER_NAMESPACE(_polyvec_invq)
void polyvec_invq(polyvec *v,
                    const uint8_t seed[KYBER_SYMBYTES],
                    uint8_t nonce,
                    const invq_table_t *tbl);

#endif /* INVQ_H */
