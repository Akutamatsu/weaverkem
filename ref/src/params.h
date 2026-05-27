#ifndef PARAMS_H
#define PARAMS_H

#ifndef WEAVER_MODE
#define WEAVER_MODE 1  /* 1: 512, 3: 1024, 5: 2048 */
#endif

#define PK_COMPRESS

#if   (WEAVER_MODE == 1)
  #define KYBER_N 256
  #define KYBER_INDCPA_MSGBYTES 16
  #define KYBER_NAMESPACE(s) pqcrystals_weaver512_ref##s
  #define KYBER_ETA1 5
  #define KYBER_ETA2 KYBER_ETA1
  #define KYBER_K 2
  #define KYBER_DV 5

  /* paper-aligned core: (n,k,q,eta1,dt,du,dv) = (256,2,3329,5,9,9,5) */
  #define KYBER_PK_POLYVECBYTES        (KYBER_K * ((KYBER_N * 9) / 8))
  #define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * ((KYBER_N * 9) / 8))
  #define KYBER_POLYCUT_DIMENSION   168 /* 128 + 40 = HIGH_CODEWORD_BITS in "msgenc.h" */

#elif (WEAVER_MODE == 3)
  #define KYBER_N 256
  #define KYBER_INDCPA_MSGBYTES       (KYBER_N / 8)
  #define KYBER_NAMESPACE(s) pqcrystals_weaver1024_ref##s
  #define KYBER_ETA1 2
  #define KYBER_ETA2 KYBER_ETA1
  #define KYBER_K 4
  #define KYBER_DV 5

  /* paper-aligned core: (n,k,q,eta1,dt,du,dv) = (256,4,3329,2,9,9,5) */
  #define KYBER_PK_POLYVECBYTES        (KYBER_K * ((KYBER_N * 9) / 8))
  #define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * ((KYBER_N * 9) / 8))
  #define KYBER_POLYCUT_DIMENSION   KYBER_N

#elif (WEAVER_MODE == 5)
  #define KYBER_N 512
  #define KYBER_INDCPA_MSGBYTES       (KYBER_N / 8)
  #define KYBER_NAMESPACE(s) pqcrystals_weaver2048_ref##s
  #define KYBER_ETA1 1
  #define KYBER_ETA2 KYBER_ETA1
  #define KYBER_K 4
  #define KYBER_DV 6

  /* paper-aligned core: (n,k,q,eta1,dt,du,dv) = (512,4,3329,1,9,9,6) */
  #define KYBER_PK_POLYVECBYTES        (KYBER_K * ((KYBER_N * 9) / 8))
  #define KYBER_POLYVECCOMPRESSEDBYTES (KYBER_K * ((KYBER_N * 9) / 8))
  #define KYBER_POLYCUT_DIMENSION   KYBER_N

#else
  #error "WEAVER_MODE must be in {1,3,5}"
#endif

#define KYBER_POLYCOMPRESSEDBYTES    ((KYBER_POLYCUT_DIMENSION * KYBER_DV) / 8)
#define KYBER_POLYBYTES     ((KYBER_N * 12) / 8)
#define KYBER_POLYVECBYTES  (KYBER_K * KYBER_POLYBYTES)

#ifndef PK_COMPRESS
#undef KYBER_PK_POLYVECBYTES
#define KYBER_PK_POLYVECBYTES KYBER_POLYVECBYTES
#endif

/* ====================================================================
 * 全局共享参数区：以下参数根据上面决定的 N 自动计算，不需要修改
 * ==================================================================== */
#define KYBER_Q     3329
#define KYBER_HALFQ ((KYBER_Q + 1) / 2)

/* size in bytes of hashes, and seeds */
#define KYBER_SYMBYTES 32
#define KYBER_KEM_DERAND_COINBYTES KYBER_INDCPA_MSGBYTES

// 确保 SSBYTES（共享密钥长度）也跟随 SYMBYTES 变动
#define KYBER_SSBYTES  KYBER_INDCPA_MSGBYTES /* size in bytes of shared key */


// 密钥与密文大小的组合计算
#define KYBER_INDCPA_PUBLICKEYBYTES (KYBER_PK_POLYVECBYTES + KYBER_SYMBYTES)
#define KYBER_INDCPA_SECRETKEYBYTES (KYBER_POLYVECBYTES)
#define KYBER_INDCPA_BYTES          ((KYBER_POLYVECCOMPRESSEDBYTES + KYBER_POLYCOMPRESSEDBYTES))

#define KYBER_PUBLICKEYBYTES  (KYBER_INDCPA_PUBLICKEYBYTES)
/* 32 bytes of additional space to save H(pk) */
#define KYBER_SECRETKEYBYTES  (KYBER_INDCPA_SECRETKEYBYTES \
                               + KYBER_INDCPA_PUBLICKEYBYTES \
                               + 2*KYBER_SYMBYTES)
#define KYBER_CIPHERTEXTBYTES  KYBER_INDCPA_BYTES


#endif
