; kern.asm - NASM win64 sample for the native platform research.
;   nasm -f win64 kern.asm -o kern.obj
;
; Contents
;   cpu_features / cpuid_raw            CPUID feature detection
;   nn_mv32x64_*                        int8 matrix-vector kernels (32 outputs, 64 inputs)
;   morton3_pdep, morton3_pdep_batch,
;   morton3_pext_x                      BMI2 3D Morton code, 3 x 10 bits
;   abi_check, clobber_*                self-test of the PROC/EPILOGUE macros
;   spin_add8                           8-cycle-per-iteration loop (core clock estimate)

bits 64
default rel
%include "win64.inc"

; ------------------------------------------------------------------ data
section .rdata rdata align=32
align 32
c_127w:     times 16 dw 127
c_perm:     dd 0, 4, 1, 5, 2, 6, 3, 7
align 16
abi_sent_gpr:
            dq 0x1111111111111101, 0x2222222222222202, 0x3333333333333303, 0x4444444444444404
            dq 0x5555555555555505, 0x6666666666666606, 0x7777777777777707, 0x8888888888888808
abi_sent_xmm:
%assign T 0
%rep 10
            dq 0xA0A0A0A0A0A0A000 + T, 0x0B0B0B0B0B0B0B00 + T
%assign T T+1
%endrep

section .bss
abi_saved_rsp:   resq 1
global abi_align_fault
abi_align_fault: resq 1
global clobber_callback
clobber_callback: resq 1

section .text

; ============================================================ CPUID
CPUF_OSYMM      equ 1 << 0      ; OS saves YMM state (OSXSAVE and XCR0[2:1] = 11b)
CPUF_AVX        equ 1 << 1
CPUF_AVX2       equ 1 << 2
CPUF_FMA        equ 1 << 3
CPUF_BMI1       equ 1 << 4
CPUF_BMI2       equ 1 << 5
CPUF_AVXVNNI    equ 1 << 6      ; VEX vpdpbusd / vpdpwssd
CPUF_VNNIINT8   equ 1 << 7      ; VEX vpdpbssd family
CPUF_VNNIINT16  equ 1 << 8
CPUF_AVX512F    equ 1 << 9      ; raw CPUID bit, reported to show its absence

%macro FEAT 3                   ; reg, bit, flag
    bt      %1, %2
    jnc     %%no
    or      r10d, %3
%%no:
%endmacro

; uint32_t cpu_features(void);   AVX-class bits are set only when the OS saves YMM state.
PROC cpu_features, 0, 0, rbx
    xor     r10d, r10d
    xor     eax, eax
    cpuid
    mov     r11d, eax               ; highest basic leaf
    mov     eax, 1
    xor     ecx, ecx
    cpuid
    mov     r8d, ecx                ; leaf 1 ECX
    bt      r8d, 27                 ; OSXSAVE
    jnc     .leaf7
    xor     ecx, ecx
    xgetbv                          ; XCR0 in edx:eax
    and     eax, 6
    cmp     eax, 6
    jne     .leaf7
    or      r10d, CPUF_OSYMM
    bt      r8d, 28                 ; AVX
    jnc     .leaf7
    or      r10d, CPUF_AVX
    FEAT    r8d, 12, CPUF_FMA
.leaf7:
    cmp     r11d, 7
    jb      .done
    mov     eax, 7
    xor     ecx, ecx
    cpuid
    mov     r11d, eax               ; highest leaf-7 subleaf
    FEAT    ebx, 3, CPUF_BMI1
    FEAT    ebx, 8, CPUF_BMI2
    FEAT    ebx, 16, CPUF_AVX512F
    test    r10d, CPUF_AVX
    jz      .done
    FEAT    ebx, 5, CPUF_AVX2
    cmp     r11d, 1
    jb      .done
    mov     eax, 7
    mov     ecx, 1
    cpuid
    FEAT    eax, 4, CPUF_AVXVNNI
    FEAT    edx, 4, CPUF_VNNIINT8
    FEAT    edx, 10, CPUF_VNNIINT16
.done:
    mov     eax, r10d
    EPILOGUE
ENDPROC

; void cpuid_raw(uint32_t leaf, uint32_t subleaf, uint32_t out[4]);  out = eax, ebx, ecx, edx
PROC cpuid_raw, 0, 0, rbx
    mov     eax, ecx
    mov     ecx, edx
    cpuid
    mov     [r8], eax
    mov     [r8 + 4], ebx
    mov     [r8 + 8], ecx
    mov     [r8 + 12], edx
    EPILOGUE
ENDPROC

; ============================================================ int8 kernels
; y[r] = clamp((b[r] + sum_c W[r][c] * x[c]) >> shift, 0, 127),  r < 32, c < 64
;   x  u8, every value 0..127       W  s8       b  s32       y  u8
; All kernels:  void f(const uint8_t *x, const int8_t *W, const int32_t *b,
;                      uint32_t shift, uint8_t *y);
;               rcx = x, rdx = W, r8 = b, r9d = shift, STK_ARG(5) = y
;
; Weight layouts
;   "c4"  (column-blocked, recommended): 2048 bytes,
;         Wc4[((c/4)*4 + r/8)*32 + (r%8)*4 + c%4] = W[r][c]
;         One 32-byte block holds 4 input columns for 8 output rows, so one
;         vpdpbusd against a broadcast input dword updates 8 rows.
;   "rm"  (row-major): Wrm[r*64 + c] = W[r][c]

; Shared tail: 4 vectors of 8 dword sums in ymm0..ymm3 -> shift, clamp, 32 bytes.
; vpackssdw saturates to s16, vpminsw caps at 127, vpackuswb floors at 0.
%macro NN_TAIL 0
    vmovd       xmm4, r9d
    vpsrad      ymm0, ymm0, xmm4
    vpsrad      ymm1, ymm1, xmm4
    vpsrad      ymm2, ymm2, xmm4
    vpsrad      ymm3, ymm3, xmm4
    vpackssdw   ymm0, ymm0, ymm1
    vpackssdw   ymm2, ymm2, ymm3
    vmovdqa     ymm5, [c_127w]
    vpminsw     ymm0, ymm0, ymm5
    vpminsw     ymm2, ymm2, ymm5
    vpackuswb   ymm0, ymm0, ymm2
    vmovdqa     ymm4, [c_perm]
    vpermd      ymm0, ymm4, ymm0
    mov         rax, STK_ARG(5)
    vmovdqu     [rax], ymm0
    vzeroupper
%endmacro

; ---- AVX-VNNI, c4 layout, %2 accumulator chains (4, 8 or 12) ------------
; Accumulators ymm0 .. ymm(%2-1); chain j of row block rb is ymm(rb + 4*j).
; The broadcast input dword lives in ymm(%2).
; %3 = 0: hand-encoded VEX vpdpbusd (needs AVX-VNNI)
; %3 = 1: "{vex} vpdpbssd", which NASM 2.16.03 does assemble (needs
;         AVX-VNNI-INT8).  Signed x signed gives the same sums because the
;         contract keeps every x byte in 0..127.
%macro NN_VNNI_C4 3
  %assign NCH  %2
  %assign CPR  NCH/4
  %assign XB   NCH
  %if NCH+1 > 6
    %assign NSAVE NCH+1-6
  %else
    %assign NSAVE 0
  %endif
PROC %1, 0, NSAVE
    vmovdqu     ymm0, [r8]
    vmovdqu     ymm1, [r8 + 32]
    vmovdqu     ymm2, [r8 + 64]
    vmovdqu     ymm3, [r8 + 96]
  %assign J 4
  %rep NCH-4
    vpxor       xmm%[J], xmm%[J], xmm%[J]
    %assign J J+1
  %endrep
  %assign G 0
  %rep 16
    vpbroadcastd ymm%[XB], [rcx + 4*G]
    %assign RB 0
    %rep 4
      %assign J RB + 4*(G % CPR)
      %if %3 == 0
      VPDPBUSD_RM ymm%[J], ymm%[XB], rdx, (G*4 + RB)*32
      %else
      {vex} vpdpbssd ymm%[J], ymm%[XB], [rdx + (G*4 + RB)*32]
      %endif
      %assign RB RB+1
    %endrep
    %assign G G+1
  %endrep
  %assign C 1
  %rep CPR-1
    %assign RB 0
    %rep 4
      %assign J RB + 4*C
      vpaddd    ymm%[RB], ymm%[RB], ymm%[J]
      %assign RB RB+1
    %endrep
    %assign C C+1
  %endrep
    NN_TAIL
    EPILOGUE
ENDPROC
%endmacro

NN_VNNI_C4 nn_mv32x64_vnni_c4x4,  4,  0
NN_VNNI_C4 nn_mv32x64_vnni_c4x8,  8,  0
NN_VNNI_C4 nn_mv32x64_vnni_c4x12, 12, 0
NN_VNNI_C4 nn_mv32x64_int8_c4x8,  8,  1

; ---- plain AVX2, c4 layout ----------------------------------------------
; vpmaddubsw (u8 x s8 -> pairs summed to s16) cannot saturate while x <= 127:
; the extreme pair sums are 2*127*127 = 32258 and 2*127*(-128) = -32512.
; vpmaddwd by ones widens pairs of s16 to s32; vpaddd accumulates.
PROC nn_mv32x64_avx2_c4, 0, 1
    vmovdqu     ymm0, [r8]
    vmovdqu     ymm1, [r8 + 32]
    vmovdqu     ymm2, [r8 + 64]
    vmovdqu     ymm3, [r8 + 96]
    vpcmpeqw    ymm6, ymm6, ymm6
    vpsrlw      ymm6, ymm6, 15              ; 16 words of 1
  %assign G 0
  %rep 16
    vpbroadcastd ymm5, [rcx + 4*G]
    %assign RB 0
    %rep 4
    vpmaddubsw  ymm4, ymm5, [rdx + (G*4 + RB)*32]
    vpmaddwd    ymm4, ymm4, ymm6
    vpaddd      ymm%[RB], ymm%[RB], ymm4
      %assign RB RB+1
    %endrep
    %assign G G+1
  %endrep
    NN_TAIL
    EPILOGUE
ENDPROC

; ---- AVX-VNNI, row-major layout (comparison only) -----------------------
; Each row is a 64-byte dot product: two vpdpbusd, then a horizontal sum.
; Eight rows are reduced together with vphaddd.
PROC nn_mv32x64_vnni_rm, 0, 8
    vmovdqu     ymm8, [rcx]
    vmovdqu     ymm9, [rcx + 32]
  %assign RB 0
  %rep 4
    %assign R 0
    %rep 8
    vpxor       xmm%[R], xmm%[R], xmm%[R]
    VPDPBUSD_RM ymm%[R], ymm8, rdx, (RB*8 + R)*64
    VPDPBUSD_RM ymm%[R], ymm9, rdx, (RB*8 + R)*64 + 32
      %assign R R+1
    %endrep
    vphaddd     ymm0, ymm0, ymm1
    vphaddd     ymm2, ymm2, ymm3
    vphaddd     ymm4, ymm4, ymm5
    vphaddd     ymm6, ymm6, ymm7
    vphaddd     ymm0, ymm0, ymm2
    vphaddd     ymm4, ymm4, ymm6
    vextracti128 xmm1, ymm0, 1
    vpaddd      xmm0, xmm0, xmm1
    vextracti128 xmm5, ymm4, 1
    vpaddd      xmm4, xmm4, xmm5
    %assign D 10+RB
    vinserti128 ymm%[D], ymm0, xmm4, 1
    vpaddd      ymm%[D], ymm%[D], [r8 + RB*32]
    %assign RB RB+1
  %endrep
    vmovdqa     ymm0, ymm10
    vmovdqa     ymm1, ymm11
    vmovdqa     ymm2, ymm12
    vmovdqa     ymm3, ymm13
    NN_TAIL
    EPILOGUE
ENDPROC

; ============================================================ Morton (BMI2)
; uint32_t morton3_pdep(uint32_t x, uint32_t y, uint32_t z);
; bit 3i = x bit i, bit 3i+1 = y bit i, bit 3i+2 = z bit i, i < 10.
; pdep takes only the low 10 source bits (the mask has 10 set bits).
; Leaf function, no stack use: needs no prologue and no unwind record.
global morton3_pdep
align 16
morton3_pdep:
    mov     eax, 0x09249249
    mov     r9d, 0x12492492
    mov     r10d, 0x24924924
    pdep    eax, ecx, eax
    pdep    edx, edx, r9d
    pdep    r8d, r8d, r10d
    or      eax, edx
    or      eax, r8d
    ret

; uint32_t morton3_pext_x(uint32_t code, uint32_t axis);  axis 0 = x, 1 = y, 2 = z
global morton3_pext_x
align 16
morton3_pext_x:
    mov     eax, 0x09249249
    shlx    eax, eax, edx
    pext    eax, ecx, eax
    ret

; void morton3_pdep_batch(const uint16_t *x, const uint16_t *y, const uint16_t *z,
;                         uint32_t *out, size_t n);
PROC morton3_pdep_batch, 0, 0, rbx, rsi, rdi, r12, r13
    mov     r10, STK_ARG(5)
    test    r10, r10
    jz      .done
    mov     ebx, 0x09249249
    mov     esi, 0x12492492
    mov     edi, 0x24924924
    xor     r11d, r11d
.loop:
    movzx   eax,  word [rcx + 2*r11]
    movzx   r12d, word [rdx + 2*r11]
    movzx   r13d, word [r8  + 2*r11]
    pdep    eax,  eax,  ebx
    pdep    r12d, r12d, esi
    pdep    r13d, r13d, edi
    or      eax, r12d
    or      eax, r13d
    mov     [r9 + 4*r11], eax
    inc     r11
    cmp     r11, r10
    jb      .loop
.done:
    EPILOGUE
ENDPROC

; ============================================================ ABI self-test
; uint32_t abi_check(void (*fn)(void));
; Loads sentinels into every callee-saved register, calls fn, and returns a
; bit mask of registers that came back changed (0 = all preserved).
;   bits 0..7  rbx rbp rsi rdi r12 r13 r14 r15     bits 8..17  xmm6..xmm15
;   bit 18     rsp changed
%macro ABI_CHK_GPR 3            ; reg, slot, bit
    cmp     %1, [rdx + 8*%2]
    je      %%ok
    or      eax, 1 << %3
%%ok:
%endmacro

PROC abi_check, 32, 10, rbx, rbp, rsi, rdi, r12, r13, r14, r15
    lea     rax, [abi_sent_gpr]
    mov     rbx, [rax]
    mov     rbp, [rax + 8]
    mov     rsi, [rax + 16]
    mov     rdi, [rax + 24]
    mov     r12, [rax + 32]
    mov     r13, [rax + 40]
    mov     r14, [rax + 48]
    mov     r15, [rax + 56]
    lea     rax, [abi_sent_xmm]
  %assign T 0
  %rep 10
    %assign U T+6
    vmovdqa xmm%[U], [rax + 16*T]
    %assign T T+1
  %endrep
    mov     [abi_saved_rsp], rsp
    call    rcx
    xor     eax, eax
    lea     rdx, [abi_sent_gpr]
    ABI_CHK_GPR rbx, 0, 0
    ABI_CHK_GPR rbp, 1, 1
    ABI_CHK_GPR rsi, 2, 2
    ABI_CHK_GPR rdi, 3, 3
    ABI_CHK_GPR r12, 4, 4
    ABI_CHK_GPR r13, 5, 5
    ABI_CHK_GPR r14, 6, 6
    ABI_CHK_GPR r15, 7, 7
    lea     rdx, [abi_sent_xmm]
  %assign T 0
  %rep 10
    %assign U T+6
    vpcmpeqb xmm0, xmm%[U], [rdx + 16*T]
    vpmovmskb ecx, xmm0
    cmp     ecx, 0xFFFF
    je      .xok%[T]
    or      eax, 1 << (8 + T)
.xok%[T]:
    %assign T T+1
  %endrep
    cmp     rsp, [abi_saved_rsp]
    je      .rspok
    or      eax, 1 << 18
.rspok:
    EPILOGUE
ENDPROC

%macro ALIGN_PROBE 0            ; records a misaligned stack
    test    rsp, 15
    jz      %%ok
    inc     qword [abi_align_fault]
%%ok:
%endmacro

%macro CALL_CB 0                ; call *clobber_callback if it is set (needs 32 bytes of locals)
    mov     rax, [clobber_callback]
    test    rax, rax
    jz      %%no
    call    rax
%%no:
%endmacro

; void clobber_all(void);  8 GPRs + 10 XMM saved, 32 bytes of locals (frame 200:
; UWOP_ALLOC_LARGE).  Destroys every callee-saved register, optionally calls
; *clobber_callback, then restores.
PROC clobber_all, 32, 10, rbx, rbp, rsi, rdi, r12, r13, r14, r15
    ALIGN_PROBE
    mov     rbx, -1
    mov     rbp, -2
    mov     rsi, -3
    mov     rdi, -4
    mov     r12, -5
    mov     r13, -6
    mov     r14, -7
    mov     r15, -8
  %assign T 6
  %rep 10
    vpcmpeqd ymm%[T], ymm%[T], ymm%[T]
    %assign T T+1
  %endrep
    vzeroupper
    CALL_CB
    EPILOGUE
ENDPROC

; 3 GPRs (odd: no pad) + 1 XMM + 32 locals: frame 48, UWOP_ALLOC_SMALL
PROC clobber_odd, 32, 1, rbx, rsi, rdi
    ALIGN_PROBE
    xor     ebx, ebx
    xor     esi, esi
    xor     edi, edi
    vpxor   xmm6, xmm6, xmm6
    CALL_CB
    EPILOGUE
ENDPROC

; 1 GPR, nothing else: frame 0, no allocation code
PROC clobber_min, 0, 0, r12
    ALIGN_PROBE
    xor     r12d, r12d
    EPILOGUE
ENDPROC

; 2 GPRs (even: 8 bytes of pad) + 3 XMM + 48 bytes of locals: frame 104
PROC clobber_even, 48, 3, r14, r15
    ALIGN_PROBE
    mov     qword [rsp + 32], 0
    mov     qword [rsp + 40], 0
    xor     r14d, r14d
    xor     r15d, r15d
    vpxor   xmm6, xmm6, xmm6
    vpxor   xmm7, xmm7, xmm7
    vpxor   xmm8, xmm8, xmm8
    CALL_CB
    EPILOGUE
ENDPROC

; 0 GPRs + 10 XMM + 32 locals: frame 200 (zero pushes is even: 8 bytes of pad)
PROC clobber_xmm, 32, 10
    ALIGN_PROBE
  %assign T 6
  %rep 10
    vpxor   xmm%[T], xmm%[T], xmm%[T]
    %assign T T+1
  %endrep
    CALL_CB
    EPILOGUE
ENDPROC

; Negative control for abi_check: breaks the ABI on purpose (r12 and xmm9 are
; destroyed and not restored).  Only ever called through abi_check, which
; restores the caller's registers itself.
global clobber_bad
align 16
clobber_bad:
    xor     r12d, r12d
    vpxor   xmm9, xmm9, xmm9
    ret

; ============================================================ clock helper
; uint64_t spin_add8(uint64_t n);  n iterations of 8 dependent register adds.
; A reg,reg add has 1 cycle of latency on every x86-64 core this targets, so
; the loop costs 8 core cycles per iteration whatever the clock frequency.
global spin_add8
align 16
spin_add8:
    xor     eax, eax
    mov     edx, 1
align 16
.l:
    add     rax, rdx
    add     rax, rdx
    add     rax, rdx
    add     rax, rdx
    add     rax, rdx
    add     rax, rdx
    add     rax, rdx
    add     rax, rdx
    dec     rcx
    jnz     .l
    ret
