#ifndef CONSTS7681_H
#define CONSTS7681_H

#include "params.h"

#define _7681_16XQ      0
#define _7681_16XQINV  32
#define _7681_V32      64
#define _7681_Q32      96
#define _7681_BIAS32  128

#ifdef __ASSEMBLER__
#if defined(__WIN32__) || defined(__APPLE__)
#define decorate7681(s) _##s
#define cdecl7681_2(s) decorate7681(s)
#define cdecl7681(s) cdecl7681_2(WEAVER_NAMESPACE(_##s))
#else
#define cdecl7681(s) WEAVER_NAMESPACE(_##s)
#endif
#else
#include <stdint.h>

typedef struct __attribute__((aligned(32))) {
  int16_t q[16];
  int16_t qinv[16];
  int32_t v[8];
  int32_t q32[8];
  int32_t bias[8];
} q7681data;

#define qdata7681 WEAVER_NAMESPACE(_qdata7681)
extern const q7681data qdata7681;
#endif

#endif
