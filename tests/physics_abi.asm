; Test the Win64 preservation and AVX return requirements on the real entry point.
; Run this probe only when the dispatcher's CPU and OS checks admit AVX2.
default rel
%include "win64.inc"
extern az_kin_axis_avx2
section .text
PROC az_test_kin_abi, 96, 10, rbx, rbp, rsi, rdi, r12, r13, r14, r15
    mov rbx, 101
    mov rbp, 102
    mov rsi, 103
    mov rdi, 104
    mov r12, 105
    mov r13, 106
    mov r14, 107
    mov r15, 108
%assign reg 6
%rep 10
    vpcmpeqd ymm%[reg], ymm%[reg], ymm%[reg]
%assign reg reg+1
%endrep
    ; Give the callee one valid lane and the caller-provided 32-byte shadow area.
    lea rcx, [rsp+64]
    lea rdx, [rsp+68]
    lea r8,  [rsp+70]
    lea r9,  [rsp+72]
    mov dword [rcx], 1
    mov word [rdx], 1
    mov word [r8], 0
    mov byte [r9], 128
    mov dword [rsp+32], 1
    mov dword [rsp+40], 16
    call az_kin_axis_avx2
    cmp rbx, 101
    jne .bad_preserve
    cmp rbp, 102
    jne .bad_preserve
    cmp rsi, 103
    jne .bad_preserve
    cmp rdi, 104
    jne .bad_preserve
    cmp r12, 105
    jne .bad_preserve
    cmp r13, 106
    jne .bad_preserve
    cmp r14, 107
    jne .bad_preserve
    cmp r15, 108
    jne .bad_preserve
%assign reg 6
%rep 10
    vpcmpeqd xmm1, xmm1, xmm1
    vpxor xmm0, xmm%[reg], xmm1
    vptest xmm0, xmm0
    jnz .bad_preserve
    vextracti128 xmm0, ymm%[reg], 1
    vptest xmm0, xmm0
    jnz .bad_upper
%assign reg reg+1
%endrep
    xor eax, eax
    jmp .done
.bad_preserve:
    mov eax, 1
    jmp .done
.bad_upper:
    mov eax, 2
.done:
    vzeroupper
    EPILOGUE
    ENDPROC
