; az_philox_avx2.asm - AVX2 twin of az_philox4x32_n (core/az_rng.c).
;
;   void az_philox_n_avx2(u32 *out4n, const u32 *ctr4n, const u32 key[2], u32 n);
;        rcx = out4n   rdx = ctr4n   r8 = key   r9d = n
;
; Law: bit-identical to the C reference for every input (tests/test_rng.c).
; Needs AVX2 only (no BMI2, no FMA). Any n, including 0 (then nothing is
; read or written, not even the key). For n > 0 it reads exactly 16*n bytes
; at ctr4n and 8 bytes at key, and writes exactly 16*n bytes at out4n.
; out4n == ctr4n (in place) is allowed, as in the reference; any other
; overlap is not. Only the low 32 bits of the n register are used.
;
; Layout. Eight blocks per iteration as two groups of four. Inside a group
; each ymm register holds one counter word of four blocks, one block per
; qword lane, the word in the low dword of the lane. The high dword is junk:
; vpmuludq reads only the low dwords and gives the full 64-bit product, so
;     L(product) = the lane itself      H(product) = lane >> 32
; and one Philox round on four blocks is 2 multiplies, 2 shifts and 4 xors:
;     c0' = H(M1 * c2) ^ c1 ^ k0        c1' = L(M1 * c2)
;     c2' = H(M0 * c0) ^ c3 ^ k1        c3' = L(M0 * c0)
; The products overwrite their own inputs, so the four words rotate through
; the four registers (word j of round r lives in register (j + r) mod 4).
;
; Tail (n mod 8 = 1..7 blocks): the remaining counters are copied into a
; zeroed 128-byte buffer on the stack, the same eight-block body runs on the
; buffer in place, and only the wanted 16*(n mod 8) bytes are copied out.
; Nothing outside the two arrays is touched.

bits 64
default rel
%include "win64.inc"

section .rdata rdata align=32
align 32
phx_w0:     times 8 dd 0x9E3779B9       ; Weyl key increments
phx_w1:     times 8 dd 0xBB67AE85
phx_m0:     dd 0xD2511F53               ; round multipliers
phx_m1:     dd 0xCD9E8D57

section .text

; ---- register plan --------------------------------------------------------
;   ymm0..ymm3    group A, counter words      ymm8   group A temporary
;   ymm4..ymm7    group B, counter words      ymm9   group B temporary
;   ymm10, ymm11  round key k0, k1 (broadcast)
;   ymm12, ymm13  M0, M1 (broadcast)
;   r10 = source of this body pass, r11 = destination, eax = 1 on the tail pass
;   locals: [rsp, rsp+128) tail buffer, [rsp+128] k0, [rsp+132] k1

%define PHX_TMP     rsp
%define PHX_K0      [rsp + 128]
%define PHX_K1      [rsp + 132]

; load four blocks at [r10 + %2] into registers %1..%1+3 (words 0..3)
%macro PHX_LOAD 2
    %assign P0 %1
    %assign P1 %1+1
    %assign P2 %1+2
    %assign P3 %1+3
    vmovdqu     xmm%[P1], [r10 + %2]                ; block 0
    vmovdqu     xmm%[P3], [r10 + %2 + 16]           ; block 1
    vinserti128 ymm%[P1], ymm%[P1], [r10 + %2 + 32], 1  ; block 0 | block 2
    vinserti128 ymm%[P3], ymm%[P3], [r10 + %2 + 48], 1  ; block 1 | block 3
    vpunpcklqdq ymm%[P0], ymm%[P1], ymm%[P3]        ; lanes = (w0, w1) of blocks 0..3
    vpunpckhqdq ymm%[P2], ymm%[P1], ymm%[P3]        ; lanes = (w2, w3) of blocks 0..3
    vpsrlq      ymm%[P1], ymm%[P0], 32              ; w1 into the low dwords
    vpsrlq      ymm%[P3], ymm%[P2], 32              ; w3 into the low dwords
%endmacro

; one round on the group whose registers start at %1, temporary %2; uses PHX_R
%macro PHX_ROUND 2
    %assign P0 %1 + ((PHX_R + 0) & 3)
    %assign P1 %1 + ((PHX_R + 1) & 3)
    %assign P2 %1 + ((PHX_R + 2) & 3)
    %assign P3 %1 + ((PHX_R + 3) & 3)
    vpmuludq    ymm%[P0], ymm%[P0], ymm12           ; M0 * c0   -> next c3
    vpmuludq    ymm%[P2], ymm%[P2], ymm13           ; M1 * c2   -> next c1
    vpxor       ymm%[P1], ymm%[P1], ymm10           ; c1 ^ k0
    vpxor       ymm%[P3], ymm%[P3], ymm11           ; c3 ^ k1
    vpsrlq      ymm%2, ymm%[P2], 32                 ; H(M1 * c2)
    vpxor       ymm%[P1], ymm%[P1], ymm%2           ; -> next c0
    vpsrlq      ymm%2, ymm%[P0], 32                 ; H(M0 * c0)
    vpxor       ymm%[P3], ymm%[P3], ymm%2           ; -> next c2
%endmacro

; store four blocks to [r11 + %2]; after 10 rounds word j is in register
; %1 + ((j + 10) & 3), that is words (0,1,2,3) are in registers (2,3,0,1)
%macro PHX_STORE 2
    %assign P0 %1+2
    %assign P1 %1+3
    %assign P2 %1+0
    %assign P3 %1+1
    vpsllq      ymm%[P1], ymm%[P1], 32
    vpsllq      ymm%[P3], ymm%[P3], 32
    vpblendd    ymm%[P0], ymm%[P0], ymm%[P1], 0xAA  ; lanes = (w0, w1)
    vpblendd    ymm%[P2], ymm%[P2], ymm%[P3], 0xAA  ; lanes = (w2, w3)
    vpunpcklqdq ymm%[P1], ymm%[P0], ymm%[P2]        ; block 0 | block 2
    vpunpckhqdq ymm%[P3], ymm%[P0], ymm%[P2]        ; block 1 | block 3
    vmovdqu     [r11 + %2], xmm%[P1]
    vmovdqu     [r11 + %2 + 16], xmm%[P3]
    vextracti128 [r11 + %2 + 32], ymm%[P1], 1
    vextracti128 [r11 + %2 + 48], ymm%[P3], 1
%endmacro

PROC az_philox_n_avx2, 144, 8
    mov         r9d, r9d                    ; n, zero-extended
    test        r9, r9
    jz          .done                       ; n = 0: touch nothing
    mov         eax, [r8]                   ; the key is read once, before any store
    mov         PHX_K0, eax
    mov         eax, [r8 + 4]
    mov         PHX_K1, eax
    vpbroadcastd ymm12, dword [phx_m0]
    vpbroadcastd ymm13, dword [phx_m1]
    xor         eax, eax                    ; not the tail pass

.loop:
    cmp         r9, 8
    jb          .tail
    mov         r10, rdx
    mov         r11, rcx

.body:                                      ; eight blocks: [r10] -> [r11]
    PHX_LOAD    0, 0
    PHX_LOAD    4, 64                       ; all 128 bytes are read before any store
    vpbroadcastd ymm10, dword PHX_K0
    vpbroadcastd ymm11, dword PHX_K1
%assign PHX_R 0
%rep 10
    PHX_ROUND   0, 8
    PHX_ROUND   4, 9
  %if PHX_R < 9
    vpaddd      ymm10, ymm10, [phx_w0]
    vpaddd      ymm11, ymm11, [phx_w1]
  %endif
  %assign PHX_R PHX_R+1
%endrep
    PHX_STORE   0, 0
    PHX_STORE   4, 64
    test        eax, eax
    jnz         .copy_out
    add         rdx, 128
    add         rcx, 128
    sub         r9, 8
    jmp         .loop

.tail:                                      ; r9 = 0..7 blocks left
    test        r9, r9
    jz          .done
    shl         r9, 4                       ; bytes left: 16..112
    vpxor       xmm0, xmm0, xmm0
    vmovdqu     [PHX_TMP], ymm0
    vmovdqu     [PHX_TMP + 32], ymm0
    vmovdqu     [PHX_TMP + 64], ymm0
    vmovdqu     [PHX_TMP + 96], ymm0
    xor         eax, eax
.copy_in:
    vmovdqu     xmm0, [rdx + rax]
    vmovdqu     [PHX_TMP + rax], xmm0
    add         rax, 16
    cmp         rax, r9
    jb          .copy_in
    mov         r10, rsp
    mov         r11, rsp
    mov         eax, 1                      ; tail pass
    jmp         .body

.copy_out:
    xor         eax, eax
.copy_out_loop:
    vmovdqu     xmm0, [PHX_TMP + rax]
    vmovdqu     [rcx + rax], xmm0
    add         rax, 16
    cmp         rax, r9
    jb          .copy_out_loop

.done:
    vzeroupper
    EPILOGUE
ENDPROC
