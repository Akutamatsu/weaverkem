#ifndef PARAMS_H
#define PARAMS_H

#ifndef WEAVER_MODE
#define WEAVER_MODE 1  /* 1: 512, 3: 1024, 5: 2048 */
#endif

#define PK_COMPRESS

#if   (WEAVER_MODE == 1)
  #define WEAVER_N 256
  #define WEAVER_INDCPA_MSGBYTES 16
  #define WEAVER_NAMESPACE(s) weaver512_ref##s
  #define WEAVER_ETA1 5
  #define WEAVER_ETA2 WEAVER_ETA1
  #define WEAVER_K 2
  #define WEAVER_DV 5

  /* paper-aligned core: (n,k,q,eta1,dt,du,dv) = (256,2,3329,5,9,9,5) */
  #define WEAVER_PK_POLYVECBYTES        (WEAVER_K * ((WEAVER_N * 9) / 8))
  #define WEAVER_POLYVECCOMPRESSEDBYTES (WEAVER_K * ((WEAVER_N * 9) / 8))
  #define WEAVER_POLYCUT_DIMENSION   168 /* 128 + 40 = HIGH_CODEWORD_BITS in "msgenc.h" */

#elif (WEAVER_MODE == 3)
  #define WEAVER_N 256
  #define WEAVER_INDCPA_MSGBYTES       (WEAVER_N / 8)
  #define WEAVER_NAMESPACE(s) weaver1024_ref##s
  #define WEAVER_ETA1 2
  #define WEAVER_ETA2 WEAVER_ETA1
  #define WEAVER_K 4
  #define WEAVER_DV 5

  /* paper-aligned core: (n,k,q,eta1,dt,du,dv) = (256,4,3329,2,9,9,5) */
  #define WEAVER_PK_POLYVECBYTES        (WEAVER_K * ((WEAVER_N * 9) / 8))
  #define WEAVER_POLYVECCOMPRESSEDBYTES (WEAVER_K * ((WEAVER_N * 9) / 8))
  #define WEAVER_POLYCUT_DIMENSION   WEAVER_N

#elif (WEAVER_MODE == 5)
  #define WEAVER_N 512
  #define WEAVER_INDCPA_MSGBYTES       (WEAVER_N / 8)
  #define WEAVER_NAMESPACE(s) weaver2048_ref##s
  #define WEAVER_ETA1 1
  #define WEAVER_ETA2 WEAVER_ETA1
  #define WEAVER_K 4
  #define WEAVER_DV 6

  /* paper-aligned core: (n,k,q,eta1,dt,du,dv) = (512,4,3329,1,9,9,6) */
  #define WEAVER_PK_POLYVECBYTES        (WEAVER_K * ((WEAVER_N * 9) / 8))
  #define WEAVER_POLYVECCOMPRESSEDBYTES (WEAVER_K * ((WEAVER_N * 9) / 8))
  #define WEAVER_POLYCUT_DIMENSION   WEAVER_N

#else
  #error "WEAVER_MODE must be in {1,3,5}"
#endif

#define WEAVER_POLYCOMPRESSEDBYTES    ((WEAVER_POLYCUT_DIMENSION * WEAVER_DV) / 8)
#define WEAVER_POLYBYTES     ((WEAVER_N * 12) / 8)
#define WEAVER_POLYVECBYTES  (WEAVER_K * WEAVER_POLYBYTES)

#ifndef PK_COMPRESS
#undef WEAVER_PK_POLYVECBYTES
#define WEAVER_PK_POLYVECBYTES WEAVER_POLYVECBYTES
#endif

/* ====================================================================
 * 全局共享参数区：以下参数根据上面决定的 N 自动计算，不需要修改
 * ==================================================================== */
#define WEAVER_Q     3329
#define WEAVER_HALFQ ((WEAVER_Q + 1) / 2)

/* size in bytes of hashes, and seeds */
#define WEAVER_SYMBYTES 32
#define WEAVER_KEM_DERAND_COINBYTES WEAVER_INDCPA_MSGBYTES

// 确保 SSBYTES（共享密钥长度）也跟随 SYMBYTES 变动
#define WEAVER_SSBYTES  WEAVER_INDCPA_MSGBYTES /* size in bytes of shared key */


// 密钥与密文大小的组合计算
#define WEAVER_INDCPA_PUBLICKEYBYTES (WEAVER_PK_POLYVECBYTES + WEAVER_SYMBYTES)
#define WEAVER_INDCPA_SECRETKEYBYTES (WEAVER_POLYVECBYTES)
#define WEAVER_INDCPA_BYTES          ((WEAVER_POLYVECCOMPRESSEDBYTES + WEAVER_POLYCOMPRESSEDBYTES))

#define WEAVER_PUBLICKEYBYTES  (WEAVER_INDCPA_PUBLICKEYBYTES)
/* 32 bytes of additional space to save H(pk) */
#define WEAVER_SECRETKEYBYTES  (WEAVER_INDCPA_SECRETKEYBYTES \
                               + WEAVER_INDCPA_PUBLICKEYBYTES \
                               + 2*WEAVER_SYMBYTES)
#define WEAVER_CIPHERTEXTBYTES  WEAVER_INDCPA_BYTES


#endif
