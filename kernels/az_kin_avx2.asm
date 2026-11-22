; Symmetric Q8 drag, one wrapping axis. Win64 register inputs
; rcx=positions, rdx=velocities, r8=dv, r9=drag; stack=n, shift.
default rel
%include "win64.inc"
section .rdata rdata align=32
align 32
kin_max: times 8 dd 16000
kin_min: times 8 dd -16000
kin_one: dd 256

; ymm0=sum, ymm1=unsigned drag, ymm2=256. Clamp before multiplying, then
; divide the signed retained impulse by 256 toward zero in every lane.
%macro KIN_STEP 0
    vpminsd ymm0, ymm0, [kin_max]
    vpmaxsd ymm0, ymm0, [kin_min]
    vpsubd  ymm1, ymm2, ymm1
    vpmulld ymm0, ymm0, ymm1
    vpsrad  ymm1, ymm0, 31
    vpsrld  ymm1, ymm1, 24
    vpaddd  ymm0, ymm0, ymm1
    vpsrad  ymm0, ymm0, 8
%endmacro

; With overlapping columns the scalar definition reads each lane after writing
; the preceding lane. Use the existing one-lane body for this case. Off the
; valid range ends, no memory is read or written by these address comparisons.
%macro FORWARD_IF_OVERLAP 4
    lea rax, [%1 + r10 * %2]
    cmp rax, %3
    jbe %%apart
    lea r11, [%3 + r10 * %4]
    cmp r11, %1
    ja .tail
%%apart:
%endmacro

section .text
PROC az_kin_axis_avx2, 0, 0
    mov r10d, STK_ARG(5)
    mov eax, STK_ARG(6)
    sub eax, 16
    vmovd xmm3, eax
    vpbroadcastd ymm2, [kin_one]
    cmp r10d, 8
    jb .tail
    FORWARD_IF_OVERLAP rdx, 2, r8, 2    ; vel and dv
    FORWARD_IF_OVERLAP rdx, 2, r9, 1    ; vel and drag
    FORWARD_IF_OVERLAP rcx, 4, r8, 2    ; pos and dv
    FORWARD_IF_OVERLAP rcx, 4, r9, 1    ; pos and drag
    FORWARD_IF_OVERLAP rcx, 4, rdx, 2   ; the two mutable spans
.groups:
    cmp r10d, 8
    jb .tail
    vpmovsxwd ymm0, [rdx]
    vpmovsxwd ymm1, [r8]
    vpaddd ymm0, ymm0, ymm1
    vpmovzxbd ymm1, [r9]
    KIN_STEP
    vpslld ymm4, ymm0, xmm3
    vpaddd ymm4, ymm4, [rcx]
    vmovdqu [rcx], ymm4
    vextracti128 xmm1, ymm0, 1
    vpackssdw xmm0, xmm0, xmm1
    vmovdqu [rdx], xmm0
    add rcx, 32
    add rdx, 16
    add r8, 16
    add r9, 8
    sub r10d, 8
    jmp .groups
.tail:
    test r10d, r10d
    jz .done
    movsx eax, word [rdx]
    movsx r11d, word [r8]
    add eax, r11d
    vmovd xmm0, eax
    movzx eax, byte [r9]
    vmovd xmm1, eax
    KIN_STEP
    vmovd eax, xmm0
    mov [rdx], ax
    vpslld xmm4, xmm0, xmm3
    vmovd eax, xmm4
    add [rcx], eax
    add rcx, 4
    add rdx, 2
    add r8, 2
    inc r9
    dec r10d
    jmp .tail
.done:
    vzeroupper
    EPILOGUE
    ENDPROC
