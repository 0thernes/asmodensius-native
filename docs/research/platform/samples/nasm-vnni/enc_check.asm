; enc_check.asm - a spread of hand-encoded VEX vpdpbusd forms.
; check_enc.sh disassembles this with llvm-objdump and compares every line
; with the operands written here (see enc_expect.txt).
bits 64
default rel
%include "win64.inc"
section .text
    VPDPBUSD_RR ymm0,  ymm1,  ymm2
    VPDPBUSD_RR ymm7,  ymm8,  ymm15
    VPDPBUSD_RR ymm8,  ymm0,  ymm7
    VPDPBUSD_RR ymm15, ymm15, ymm15
    VPDPBUSD_RR ymm12, ymm3,  ymm9
    VPDPBUSD_RM ymm0,  ymm1,  rdx, 0
    VPDPBUSD_RM ymm3,  ymm12, rdx, 32
    VPDPBUSD_RM ymm3,  ymm12, rdx, 127
    VPDPBUSD_RM ymm3,  ymm12, rdx, 128
    VPDPBUSD_RM ymm11, ymm8,  rdx, 2016
    VPDPBUSD_RM ymm9,  ymm4,  rcx, -64
    VPDPBUSD_RM ymm9,  ymm4,  rcx, -129
    VPDPBUSD_RM ymm2,  ymm5,  rbp, 0
    VPDPBUSD_RM ymm2,  ymm5,  r13, 0
    VPDPBUSD_RM ymm14, ymm6,  r8,  0
    VPDPBUSD_RM ymm14, ymm6,  r15, 4096
    VPDPBUSD_RM ymm1,  ymm2,  rax, 8
    VPDPBUSD_RM ymm1,  ymm2,  rsi, 8
    VPDPBUSD_RM ymm1,  ymm2,  rdi, 8
    VPDPBUSD_RM ymm1,  ymm2,  rbx, 8
    VPDPBUSD_RM ymm1,  ymm2,  r9,  8
    VPDPBUSD_RM ymm1,  ymm2,  r10, 8
    VPDPBUSD_RM ymm1,  ymm2,  r11, 8
    VPDPBUSD_RM ymm1,  ymm2,  r14, 8
