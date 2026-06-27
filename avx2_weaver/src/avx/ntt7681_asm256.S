#include "params.h"
#include "avx/consts7681.h"
#include "avx/ntt7681_macros.inc"
#include "avx/ntt7681_asm_gen.inc"

.text

/* void ntt7681_asm256(int16_t *r, const int16_t *zetas); */
.global cdecl7681(ntt7681_asm256)
cdecl7681(ntt7681_asm256):
.cfi_startproc
        push    %rbp
        push    %rbx
        push    %r12
        sub     $8, %rsp
        .cfi_def_cfa_offset 40
        mov     %rsi, %rbx
        mov     %rdi, %r12
        NTT7681_LOAD_CONST
        NTT7681_ASM_FORWARD_SIMD
        mov     %r12, %rdi
        leaq    128(%rbx), %rsi
        movl    $64, %edx
        call    cdecl7681(ntt7681_tail_lazy_len2)
        mov     %r12, %rdi
        leaq    256(%rbx), %rsi
        movl    $128, %edx
        call    cdecl7681(ntt7681_tail_full_len1)
        add     $8, %rsp
        pop     %r12
        pop     %rbx
        pop     %rbp
        .cfi_def_cfa_offset 8
        ret
.cfi_endproc

/* void invntt7681_asm256(int16_t *r, const int16_t *zetas); */
.global cdecl7681(invntt7681_asm256)
cdecl7681(invntt7681_asm256):
.cfi_startproc
        push    %rbp
        push    %rbx
        push    %r12
        sub     $8, %rsp
        .cfi_def_cfa_offset 40
        mov     %rsi, %rbx
        mov     %rdi, %r12
        NTT7681_LOAD_CONST
        mov     %r12, %rdi
        leaq    256(%rbx), %rsi
        movl    $128, %edx
        call    cdecl7681(ntt7681_tail_inv_lazy_len1)
        mov     %r12, %rdi
        leaq    128(%rbx), %rsi
        movl    $64, %edx
        call    cdecl7681(ntt7681_tail_inv_full_len2)
        mov     %r12, %rdi
        mov     %rbx, %rsi
        NTT7681_ASM_INVERSE_SIMD
        mov     %r12, %rdi
        call    cdecl7681(ntt7681_tail_scale)
        add     $8, %rsp
        pop     %r12
        pop     %rbx
        pop     %rbp
        .cfi_def_cfa_offset 8
        ret
.cfi_endproc

#if defined(__linux__) && defined(__ELF__)
.section .note.GNU-stack,"",%progbits
#endif
