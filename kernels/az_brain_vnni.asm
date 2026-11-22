; az_brain_vnni.asm - assembly twins of az_brain_step_c (core/az_brain.c):
; one step of a creature's own brain, a sparse int8 network of any size.
;
;   void az_brain_step_vnni(const AzBrain *b, u8 *mind, i8 *motor);
;   void az_brain_step_avx2(same);
;
; mind = sens[4 * n_sg] ++ hid[8 * n_hb] ++ hid2[8 * n_hb]. For every hidden
; block of 8 units (one ymm of dwords):
;   acc      = b1[block]
;   acc     += for each tile of the block: dot4(x[in group], weights)   x = sens ++ hid
;   a        = (((acc >> shift1) * gain) >> 6) + 2 * bias               lane = unit & 7
;   hid2     = clamp(a, 0, 127)
; then for every motor block the same sum over hid2, and
;   motor    = clamp(acc >> shift2, -127, 127)
; All dword arithmetic wraps, as the reference's does.
;
; A tile is 32 signed bytes: dword k holds output k's four weights. The four
; input bytes are broadcast to 8 dwords; one vpdpbusd (or vpmaddubsw +
; vpmaddwd + vpaddd) then updates the 8 outputs of the block.
;
;   vnni twin: VEX vpdpbusd (u8 x s8), hand-encoded (see win64.inc). Exact for
;              every input byte. Needs AVX2 + AVX-VNNI.
;   avx2 twin: vpmaddubsw cannot saturate while every input is <= 127
;              (extreme pair sums are 32258 and -32512); the contract of the
;              step guarantees that for sens and hid, the clamp for hid2.
;
; Why the clamps are exact: vpackssdw saturates to s16 (monotone), then
; vpminsw 127 and vpackuswb give clamp(.., 0, 127); for the motors vpminsw
; 127, vpmaxsw -127 and vpacksswb. The loops' trip counts come from the brain
; header alone, never from the data.

bits 64
default rel
%include "win64.inc"

; AzBrain field offsets, pinned by _Static_assert in kernels/az_kernels.c
BR_N_HB     equ 0
BR_N_MB     equ 2
BR_SHIFT1   equ 12
BR_SHIFT2   equ 13
BR_N_SG     equ 14
BR_GAIN     equ 16
BR_BIAS     equ 24
BR_OFF_B1   equ 32
BR_OFF_B2   equ 36
BR_OFF_C1   equ 40
BR_OFF_C2   equ 44
BR_OFF_IN1  equ 48
BR_OFF_IN2  equ 52
BR_OFF_W1   equ 56
BR_OFF_W2   equ 60

section .rdata rdata align=32
align 32
bt_127w:    times 16 dw 127
bt_m127w:   times 16 dw -127

section .text

; ymm0 += dot4(ymm1 as u8, 32 signed bytes at [rdi]) per dword
;   %1 mode (0 vnni, 1 avx2); avx2 uses ymm5 as scratch and 16 words of 1 in ymm6
%macro BT_DOT 1
  %if %1 == 0
    VPDPBUSD_RM ymm0, ymm1, rdi, 0
  %else
    vpmaddubsw  ymm5, ymm1, [rdi]
    vpmaddwd    ymm5, ymm5, ymm6
    vpaddd      ymm0, ymm0, ymm5
  %endif
%endmacro

; %1 name, %2 mode (0 vnni, 1 avx2)
%macro BRAIN_TILE_STEP 2
  %if %2 == 0
    %assign BT_NSAVE 0
  %else
    %assign BT_NSAVE 1
  %endif
PROC %1, 0, BT_NSAVE, rbx, rsi, rdi, r12
    ; rcx = brain, rdx = mind, r8 = motor
  %if %2 == 1
    vpcmpeqw    ymm6, ymm6, ymm6
    vpsrlw      ymm6, ymm6, 15              ; 16 words of 1
  %endif
    movzx       r9d, word [rcx + BR_N_HB]
    movzx       eax, byte [rcx + BR_N_SG]
    lea         rax, [rdx + rax*4]
    lea         rax, [rax + r9*8]           ; hid2 = mind + 4 * n_sg + 8 * n_hb
    mov         r10d, [rcx + BR_OFF_B1]
    add         r10, rcx
    mov         r11d, [rcx + BR_OFF_C1]
    add         r11, rcx
    mov         esi, [rcx + BR_OFF_IN1]
    add         rsi, rcx
    mov         edi, [rcx + BR_OFF_W1]
    add         rdi, rcx
    vpmovzxbd   ymm2, qword [rcx + BR_GAIN] ; gain per lane
    vpmovsxbd   ymm3, qword [rcx + BR_BIAS]
    vpaddd      ymm3, ymm3, ymm3            ; 2 * bias
    movzx       ebx, byte [rcx + BR_SHIFT1]
    and         ebx, 31
    vmovd       xmm4, ebx
    test        r9d, r9d
    jz          .l1_done
.l1_block:
    vmovdqu     ymm0, [r10]                 ; the block's 8 biases
    movzx       ebx, word [r11]             ; its tiles
    test        ebx, ebx
    jz          .l1_fin
.l1_tile:
    movzx       r12d, word [rsi]
    vpbroadcastd ymm1, [rdx + r12*4]
    BT_DOT      %2
    add         rsi, 2
    add         rdi, 32
    dec         ebx
    jnz         .l1_tile
.l1_fin:
    vpsrad      ymm0, ymm0, xmm4
    vpmulld     ymm0, ymm0, ymm2
    vpsrad      ymm0, ymm0, 6
    vpaddd      ymm0, ymm0, ymm3
    vpackssdw   ymm0, ymm0, ymm0            ; lane 0: units 0..3 (twice), lane 1: units 4..7
    vpminsw     ymm0, ymm0, [bt_127w]
    vpackuswb   ymm0, ymm0, ymm0
    vextracti128 xmm1, ymm0, 1
    vmovd       dword [rax], xmm0
    vmovd       dword [rax + 4], xmm1
    add         rax, 8
    add         r10, 32
    add         r11, 2
    dec         r9d
    jnz         .l1_block
.l1_done:
    ; ---- layer 2: the inputs are hid2 ------------------------------------
    movzx       r9d, word [rcx + BR_N_HB]
    movzx       eax, byte [rcx + BR_N_SG]
    lea         rdx, [rdx + rax*4]
    lea         rdx, [rdx + r9*8]           ; hid2
    movzx       r9d, word [rcx + BR_N_MB]
    mov         r10d, [rcx + BR_OFF_B2]
    add         r10, rcx
    mov         r11d, [rcx + BR_OFF_C2]
    add         r11, rcx
    mov         esi, [rcx + BR_OFF_IN2]
    add         rsi, rcx
    mov         edi, [rcx + BR_OFF_W2]
    add         rdi, rcx
    movzx       ebx, byte [rcx + BR_SHIFT2]
    and         ebx, 31
    vmovd       xmm4, ebx
    test        r9d, r9d
    jz          .done
.l2_block:
    vmovdqu     ymm0, [r10]
    movzx       ebx, word [r11]
    test        ebx, ebx
    jz          .l2_fin
.l2_tile:
    movzx       r12d, word [rsi]
    vpbroadcastd ymm1, [rdx + r12*4]
    BT_DOT      %2
    add         rsi, 2
    add         rdi, 32
    dec         ebx
    jnz         .l2_tile
.l2_fin:
    vpsrad      ymm0, ymm0, xmm4
    vpackssdw   ymm0, ymm0, ymm0
    vpminsw     ymm0, ymm0, [bt_127w]
    vpmaxsw     ymm0, ymm0, [bt_m127w]
    vpacksswb   ymm0, ymm0, ymm0
    vextracti128 xmm1, ymm0, 1
    vmovd       dword [r8], xmm0
    vmovd       dword [r8 + 4], xmm1
    add         r8, 8
    add         r10, 32
    add         r11, 2
    dec         r9d
    jnz         .l2_block
.done:
    vzeroupper
    EPILOGUE
ENDPROC
%endmacro

BRAIN_TILE_STEP az_brain_step_vnni, 0
BRAIN_TILE_STEP az_brain_step_avx2, 1
