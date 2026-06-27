#include "consts7681.h"

/* r8=qdata7681, r12=poly, ymm0=q, ymm1=qinv, ymm2=v32, ymm6=q32, ymm7=bias32 */

.macro NTT7681_LOAD_CONST
leaq            cdecl7681(qdata7681)(%rip), %r8
vmovdqu         _7681_16XQ(%r8), %ymm0
vmovdqu         _7681_16XQINV(%r8), %ymm1
vmovdqu         _7681_V32(%r8), %ymm2
vmovdqu         _7681_Q32(%r8), %ymm6
vmovdqu         _7681_BIAS32(%r8), %ymm7
.endm

.macro LOAD_ZETA zidx
movzwl          (\zidx)*2(%rsi), %r10d
vpbroadcastw    %r10d, %ymm15
.endm

.macro BARRETT_YMM4 dst
vextracti128    $0, \dst, %xmm8
vextracti128    $1, \dst, %xmm9
vpmovsxwd       %xmm8, %ymm8
vpmovsxwd       %xmm9, %ymm9
vpmulld         %ymm2, %ymm8, %ymm10
vpmulld         %ymm2, %ymm9, %ymm11
vpaddd          %ymm7, %ymm10, %ymm10
vpaddd          %ymm7, %ymm11, %ymm11
vpsrad          $26, %ymm10, %ymm10
vpsrad          $26, %ymm11, %ymm11
vpmulld         %ymm6, %ymm10, %ymm10
vpmulld         %ymm6, %ymm11, %ymm11
vpsubd          %ymm10, %ymm8, %ymm8
vpsubd          %ymm11, %ymm9, %ymm9
vextracti128    $1, %ymm8, %xmm10
vpackssdw       %xmm10, %xmm8, %xmm8
vextracti128    $1, %ymm9, %xmm11
vpackssdw       %xmm11, %xmm9, %xmm9
vinserti128     $1, %xmm9, %ymm8, \dst
.endm

.macro BARRETT_XMM4 dst
vmovdqa         \dst, %xmm8
vpmovsxwd       %xmm8, %ymm8
vpmulld         %ymm2, %ymm8, %ymm10
vpaddd          %ymm7, %ymm10, %ymm10
vpsrad          $26, %ymm10, %ymm10
vpmulld         %ymm6, %ymm10, %ymm10
vpsubd          %ymm10, %ymm8, %ymm8
vextracti128    $1, %ymm8, %xmm9
vpackssdw       %xmm9, %xmm8, \dst
.endm

.macro MONTMUL_XMM5
vpmullw         %xmm15, %xmm5, %xmm12
vpmulhw         %xmm15, %xmm5, %xmm13
vextracti128    $0, %ymm1, %xmm11
vextracti128    $0, %ymm0, %xmm10
vpmullw         %xmm11, %xmm12, %xmm12
vpmulhw         %xmm10, %xmm12, %xmm12
vpsubw          %xmm12, %xmm13, %xmm5
.endm

.macro BF_LAZY_YMM start, len, zidx
LOAD_ZETA       \zidx
vmovdqu         (\start)*2(%r12), %ymm4
vmovdqu         ((\start)+(\len))*2(%r12), %ymm5
vpmullw         %ymm15, %ymm5, %ymm12
vpmulhw         %ymm15, %ymm5, %ymm13
vpmullw         %ymm1, %ymm12, %ymm12
vpmulhw         %ymm0, %ymm12, %ymm12
vpsubw          %ymm12, %ymm13, %ymm5
vpsubw          %ymm5, %ymm4, %ymm12
vpaddw          %ymm5, %ymm4, %ymm4
vmovdqu         %ymm12, ((\start)+(\len))*2(%r12)
vmovdqu         %ymm4, (\start)*2(%r12)
.endm

.macro BF_FULL_YMM start, len, zidx
LOAD_ZETA       \zidx
vmovdqu         (\start)*2(%r12), %ymm4
vmovdqu         ((\start)+(\len))*2(%r12), %ymm5
vpmullw         %ymm15, %ymm5, %ymm12
vpmulhw         %ymm15, %ymm5, %ymm13
vpmullw         %ymm1, %ymm12, %ymm12
vpmulhw         %ymm0, %ymm12, %ymm12
vpsubw          %ymm12, %ymm13, %ymm5
vpsubw          %ymm5, %ymm4, %ymm12
vpaddw          %ymm5, %ymm4, %ymm4
vmovdqu         %ymm12, %ymm5
BARRETT_YMM4    %ymm5
vmovdqu         %ymm5, ((\start)+(\len))*2(%r12)
BARRETT_YMM4    %ymm4
vmovdqu         %ymm4, (\start)*2(%r12)
.endm

.macro BF_LAZY_XMM4 start, len, zidx
LOAD_ZETA       \zidx
vmovq           (\start)*2(%r12), %xmm4
vmovq           ((\start)+(\len))*2(%r12), %xmm5
MONTMUL_XMM5
vpsubw          %xmm5, %xmm4, %xmm12
vpaddw          %xmm5, %xmm4, %xmm4
vmovq           %xmm12, ((\start)+(\len))*2(%r12)
vmovq           %xmm4, (\start)*2(%r12)
.endm

.macro BF_LAZY_XMM8 start, len, zidx
LOAD_ZETA       \zidx
vmovdqu         (\start)*2(%r12), %xmm4
vmovdqu         ((\start)+(\len))*2(%r12), %xmm5
MONTMUL_XMM5
vpsubw          %xmm5, %xmm4, %xmm12
vpaddw          %xmm5, %xmm4, %xmm4
vmovdqu         %xmm12, ((\start)+(\len))*2(%r12)
vmovdqu         %xmm4, (\start)*2(%r12)
.endm

.macro BF_FULL_XMM4 start, len, zidx
LOAD_ZETA       \zidx
vmovq           (\start)*2(%r12), %xmm4
vmovq           ((\start)+(\len))*2(%r12), %xmm5
MONTMUL_XMM5
vpsubw          %xmm5, %xmm4, %xmm12
vpaddw          %xmm5, %xmm4, %xmm4
vmovq           %xmm12, %xmm5
BARRETT_XMM4    %xmm5
vmovq           %xmm5, ((\start)+(\len))*2(%r12)
BARRETT_XMM4    %xmm4
vmovq           %xmm4, (\start)*2(%r12)
.endm

.macro BF_FULL_XMM8 start, len, zidx
vmovdqu         (\start)*2(%r12), %xmm4
vmovdqu         ((\start)+(\len))*2(%r12), %xmm5
LOAD_ZETA       \zidx
MONTMUL_XMM5
vpsubw          %xmm5, %xmm4, %xmm12
vpaddw          %xmm5, %xmm4, %xmm4
vmovdqu         %xmm12, ((\start)+(\len))*2(%r12)
vmovdqu         %xmm4, (\start)*2(%r12)
vmovdqu         ((\start)+(\len))*2(%r12), %xmm4
BARRETT_XMM4    %xmm4
vmovdqu         %xmm4, ((\start)+(\len))*2(%r12)
vmovdqu         (\start)*2(%r12), %xmm4
BARRETT_XMM4    %xmm4
vmovdqu         %xmm4, (\start)*2(%r12)
.endm

.macro BF_INV_LAZY_YMM start, len, zidx
LOAD_ZETA       \zidx
vmovdqu         (\start)*2(%r12), %ymm4
vmovdqu         ((\start)+(\len))*2(%r12), %ymm5
vpaddw          %ymm5, %ymm4, %ymm8
vpsubw          %ymm4, %ymm5, %ymm5
vpmullw         %ymm15, %ymm5, %ymm12
vpmulhw         %ymm15, %ymm5, %ymm13
vpmullw         %ymm1, %ymm12, %ymm12
vpmulhw         %ymm0, %ymm12, %ymm12
vpsubw          %ymm12, %ymm13, %ymm5
vmovdqu         %ymm8, (\start)*2(%r12)
vmovdqu         %ymm5, ((\start)+(\len))*2(%r12)
.endm

.macro BF_INV_FULL_YMM start, len, zidx
LOAD_ZETA       \zidx
vmovdqu         (\start)*2(%r12), %ymm4
vmovdqu         ((\start)+(\len))*2(%r12), %ymm5
vpaddw          %ymm5, %ymm4, %ymm8
vpsubw          %ymm4, %ymm5, %ymm5
vmovdqu         %ymm8, %ymm4
BARRETT_YMM4    %ymm4
vmovdqu         %ymm4, (\start)*2(%r12)
BARRETT_YMM4    %ymm5
vpmullw         %ymm15, %ymm5, %ymm12
vpmulhw         %ymm15, %ymm5, %ymm13
vpmullw         %ymm1, %ymm12, %ymm12
vpmulhw         %ymm0, %ymm12, %ymm12
vpsubw          %ymm12, %ymm13, %ymm5
vmovdqu         %ymm5, ((\start)+(\len))*2(%r12)
.endm

.macro BF_INV_LAZY_XMM4 start, len, zidx
LOAD_ZETA       \zidx
vmovq           (\start)*2(%r12), %xmm4
vmovq           ((\start)+(\len))*2(%r12), %xmm5
vpaddw          %xmm5, %xmm4, %xmm8
vpsubw          %xmm4, %xmm5, %xmm5
MONTMUL_XMM5
vmovq           %xmm8, (\start)*2(%r12)
vmovq           %xmm5, ((\start)+(\len))*2(%r12)
.endm

.macro BF_INV_LAZY_XMM8 start, len, zidx
vmovdqu         (\start)*2(%r12), %xmm4
vmovdqu         ((\start)+(\len))*2(%r12), %xmm5
LOAD_ZETA       \zidx
vpaddw          %xmm5, %xmm4, %xmm8
vpsubw          %xmm4, %xmm5, %xmm5
MONTMUL_XMM5
vmovdqu         %xmm8, (\start)*2(%r12)
vmovdqu         %xmm5, ((\start)+(\len))*2(%r12)
.endm

.macro BF_INV_FULL_XMM8 start, len, zidx
LOAD_ZETA       \zidx
vmovdqu         (\start)*2(%r12), %xmm4
vmovdqu         ((\start)+(\len))*2(%r12), %xmm5
vpaddw          %xmm5, %xmm4, %xmm8
vpsubw          %xmm4, %xmm5, %xmm5
vmovdqu         %xmm8, %xmm4
BARRETT_XMM4    %xmm4
vmovdqu         %xmm4, (\start)*2(%r12)
BARRETT_XMM4    %xmm5
MONTMUL_XMM5
vmovdqu         %xmm5, ((\start)+(\len))*2(%r12)
.endm

.macro BF_INV_FULL_XMM4 start, len, zidx
LOAD_ZETA       \zidx
vmovq           (\start)*2(%r12), %xmm4
vmovq           ((\start)+(\len))*2(%r12), %xmm5
vpaddw          %xmm5, %xmm4, %xmm8
vpsubw          %xmm4, %xmm5, %xmm5
vmovq           %xmm8, %xmm4
BARRETT_XMM4    %xmm4
vmovq           %xmm4, (\start)*2(%r12)
BARRETT_XMM4    %xmm5
MONTMUL_XMM5
vmovq           %xmm5, ((\start)+(\len))*2(%r12)
.endm
