; az_dense_vnni.asm - assembly twins of az_dense_step_c (kernels/az_dense.c).
;
;   void az_dense_step_vnni(const AzDenseNet *c, const u8 *sens, u8 *hid,
;                           const u8 *gain, const i8 *bias, i8 *motor);
;   void az_dense_step_avx2(same);
;
; Both are bit-identical to the C reference on the whole contract:
;   sens, hid 0..127; gain any u8; bias any i8; weights any i8;
;   |b1|, |b2| <= 2^20; shifts 0..15.
;
;   acc[h]   = b1[h] + sum_j w1[h][j] * x[j]      x = sens[0..47] ++ hid[0..31]
;   a        = ((acc[h] >> shift1) * gain[h & 7] >> 6) + 2 * bias[h & 7]
;   hid'[h]  = clamp(a, 0, 127)
;   m[k]     = b2[k] + sum_h w2[k][h] * hid'[h]
;   motor[k] = clamp(m[k] >> shift2, -127, 127)
;
; Weight layout (AZ_W1_IDX, AZ_W2_IDX): per group of 4 inputs one block of
; dwords, one dword per output with its 4 weights. A broadcast input dword
; against a 32-byte weight block updates 8 outputs.
;
;   vnni twin: VEX vpdpbusd (u8 x s8), hand-encoded because NASM 2.16.03 only
;              knows the EVEX form (see win64.inc). Needs AVX2 + AVX-VNNI.
;   avx2 twin: vpmaddubsw + vpmaddwd + vpaddd. vpmaddubsw cannot saturate
;              while every input is <= 127 (extreme pair sums are 32258 and
;              -32512), which the contract guarantees for sens and hid, and
;              the clamp guarantees for hid'.
;
; Why the modulation is exact: after the shift |a| < 2^22 and gain <= 255, so
; vpmulld's low 32 bits are the whole product. Why the clamps are exact:
; vpackssdw saturates to s16 (monotone), then vpminsw 127 / vpackuswb give
; clamp(.., 0, 127); for the motors vpminsw 127 / vpmaxsw -127 / vpacksswb.
;
; hid is updated in place: every layer-1 load of hid happens before the one
; store of hid', and layer 2 reads hid' back from that store.
; Straight-line code, no branches, no data-dependent timing.

bits 64
default rel
%include "win64.inc"

; AzDenseNet field offsets, pinned by _Static_assert in kernels/az_dense.c
CL_W1       equ 0
CL_W2       equ 2560
CL_B1       equ 3072
CL_B2       equ 3200
CL_SHIFT1   equ 3264
CL_SHIFT2   equ 3265

NGROUP1     equ 20              ; (48 + 32) / 4 input dwords of layer 1
NSENSG      equ 12              ; of which the first 12 come from sens
NGROUP2     equ 8               ; 32 / 4 input dwords of layer 2

section .rdata rdata align=32
align 32
bc_127w:    times 16 dw 127
bc_m127w:   times 16 dw -127
bc_perm:    dd 0, 4, 1, 5, 2, 6, 3, 7

section .text

; acc += dot4(x_u8, weights at [base + disp]) per dword
;   %1 mode (0 vnni, 1 avx2), %2 acc, %3 x, %4 base, %5 disp
;   avx2 mode uses ymm4 as scratch and expects 16 words of 1 in ymm6
%macro BR_DOT 5
  %if %1 == 0
    VPDPBUSD_RM %2, %3, %4, %5
  %else
    vpmaddubsw  ymm4, %3, [%4 + %5]
    vpmaddwd    ymm4, ymm4, ymm6
    vpaddd      %2, %2, ymm4
  %endif
%endmacro

; %1 name, %2 mode (0 vnni, 1 avx2)
; vnni: 8 accumulator chains ymm0..7 (row block rb, chain g & 1), x in ymm8.
; avx2: 4 accumulators ymm0..3, scratch ymm4, x in ymm5, ones in ymm6.
%macro BRAIN_STEP 2
  %if %2 == 0
    %assign BR_NSAVE 3
    %assign BR_XN 8
  %else
    %assign BR_NSAVE 1
    %assign BR_XN 5
  %endif
PROC %1, 0, BR_NSAVE
    ; rcx = clade, rdx = sens, r8 = hid, r9 = gain, STK_ARG(5) = bias, STK_ARG(6) = motor
    vmovdqu     ymm0, [rcx + CL_B1]
    vmovdqu     ymm1, [rcx + CL_B1 + 32]
    vmovdqu     ymm2, [rcx + CL_B1 + 64]
    vmovdqu     ymm3, [rcx + CL_B1 + 96]
  %if %2 == 0
    vpxor       xmm4, xmm4, xmm4
    vpxor       xmm5, xmm5, xmm5
    vpxor       xmm6, xmm6, xmm6
    vpxor       xmm7, xmm7, xmm7
  %else
    vpcmpeqw    ymm6, ymm6, ymm6
    vpsrlw      ymm6, ymm6, 15              ; 16 words of 1
  %endif

    ; ---- layer 1: 20 input dwords x 4 row blocks -------------------------
  %assign G 0
  %rep NGROUP1
    %if G < NSENSG
    vpbroadcastd ymm%[BR_XN], [rdx + 4*G]
    %else
    vpbroadcastd ymm%[BR_XN], [r8 + 4*(G - NSENSG)]
    %endif
    %assign RB 0
    %rep 4
      %if %2 == 0
        %assign J RB + 4*(G & 1)
      %else
        %assign J RB
      %endif
      BR_DOT %2, ymm%[J], ymm%[BR_XN], rcx, CL_W1 + (G*4 + RB)*32
      %assign RB RB+1
    %endrep
    %assign G G+1
  %endrep
  %if %2 == 0
    vpaddd      ymm0, ymm0, ymm4
    vpaddd      ymm1, ymm1, ymm5
    vpaddd      ymm2, ymm2, ymm6
    vpaddd      ymm3, ymm3, ymm7
  %endif

    ; ---- shift, modulation, clamp to 0..127, store hid' -------------------
    movzx       eax, byte [rcx + CL_SHIFT1]
    vmovd       xmm4, eax
    vpsrad      ymm0, ymm0, xmm4
    vpsrad      ymm1, ymm1, xmm4
    vpsrad      ymm2, ymm2, xmm4
    vpsrad      ymm3, ymm3, xmm4
    vpmovzxbd   ymm4, qword [r9]            ; gain[h & 7]: lane = h & 7
    vpmulld     ymm0, ymm0, ymm4
    vpmulld     ymm1, ymm1, ymm4
    vpmulld     ymm2, ymm2, ymm4
    vpmulld     ymm3, ymm3, ymm4
    mov         rax, STK_ARG(5)
    vpmovsxbd   ymm5, qword [rax]           ; bias[h & 7]
    vpaddd      ymm5, ymm5, ymm5            ; 2 * bias
    vpsrad      ymm0, ymm0, 6
    vpsrad      ymm1, ymm1, 6
    vpsrad      ymm2, ymm2, 6
    vpsrad      ymm3, ymm3, 6
    vpaddd      ymm0, ymm0, ymm5
    vpaddd      ymm1, ymm1, ymm5
    vpaddd      ymm2, ymm2, ymm5
    vpaddd      ymm3, ymm3, ymm5
    vpackssdw   ymm0, ymm0, ymm1
    vpackssdw   ymm2, ymm2, ymm3
    vmovdqa     ymm5, [bc_127w]
    vpminsw     ymm0, ymm0, ymm5
    vpminsw     ymm2, ymm2, ymm5
    vpackuswb   ymm0, ymm0, ymm2
    vmovdqa     ymm4, [bc_perm]
    vpermd      ymm0, ymm4, ymm0            ; undo the lane interleave of the packs
    vmovdqu     [r8], ymm0                  ; hid' (the only write to hid)

    ; ---- layer 2: 8 input dwords x 2 row blocks ---------------------------
    vmovdqu     ymm0, [rcx + CL_B2]
    vmovdqu     ymm1, [rcx + CL_B2 + 32]
  %if %2 == 0
    vpxor       xmm2, xmm2, xmm2
    vpxor       xmm3, xmm3, xmm3
  %endif
  %assign G 0
  %rep NGROUP2
    vpbroadcastd ymm%[BR_XN], [r8 + 4*G]
    %assign RB 0
    %rep 2
      %if %2 == 0
        %assign J RB + 2*(G & 1)
      %else
        %assign J RB
      %endif
      BR_DOT %2, ymm%[J], ymm%[BR_XN], rcx, CL_W2 + (G*2 + RB)*32
      %assign RB RB+1
    %endrep
    %assign G G+1
  %endrep
  %if %2 == 0
    vpaddd      ymm0, ymm0, ymm2
    vpaddd      ymm1, ymm1, ymm3
  %endif

    ; ---- shift, clamp to -127..127, store motor ---------------------------
    movzx       eax, byte [rcx + CL_SHIFT2]
    vmovd       xmm4, eax
    vpsrad      ymm0, ymm0, xmm4
    vpsrad      ymm1, ymm1, xmm4
    vpackssdw   ymm0, ymm0, ymm1
    vpminsw     ymm0, ymm0, [bc_127w]
    vpmaxsw     ymm0, ymm0, [bc_m127w]
    vpacksswb   ymm0, ymm0, ymm0
    vmovdqa     ymm4, [bc_perm]
    vpermd      ymm0, ymm4, ymm0
    mov         rax, STK_ARG(6)
    vmovdqu     [rax], xmm0
    vzeroupper
    EPILOGUE
ENDPROC
%endmacro

BRAIN_STEP az_dense_step_vnni, 0
BRAIN_STEP az_dense_step_avx2, 1
