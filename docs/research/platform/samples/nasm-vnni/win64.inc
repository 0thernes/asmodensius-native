; win64.inc - NASM macros for the Microsoft x64 calling convention.
;
;   PROC name, locals, nxmm [, gpr, gpr, ...]
;       name    function label (made global)
;       locals  bytes of local stack below the saved XMM area (rounded up to 16).
;               Give 32 or more if the function calls anything (shadow space).
;       nxmm    how many callee-saved XMM registers to save, always xmm6 upward
;               (nxmm = 3 saves xmm6, xmm7, xmm8).  0..10.  Needs AVX (vmovaps).
;       gpr...  callee-saved GPRs to save: any of rbx rbp rsi rdi r12 r13 r14 r15
;   EPILOGUE    restore everything and ret (may be used more than once)
;   ENDPROC     ends the function and emits its .pdata/.xdata unwind records
;
; After PROC:   rsp is 16-byte aligned,
;               [rsp .. rsp+locals)                  locals
;               [rsp+locals+16*i]                    saved xmm(6+i), aligned
;               STK_ARG(n), n >= 5                   the n-th argument
;               HOME(n), n = 1..4                    caller's home slot n (free scratch)
;
; Layout proof: at entry rsp = 8 (mod 16).  After k pushes rsp = 8+8k (mod 16).
; The frame PRO_FRAME = locals + 16*nxmm + (8 if k is even) brings it to 0 (mod 16).

%ifndef WIN64_INC
%define WIN64_INC

; ---- register-name -> number tables -------------------------------------
%define GN_rax 0
%define GN_rcx 1
%define GN_rdx 2
%define GN_rbx 3
%define GN_rsp 4
%define GN_rbp 5
%define GN_rsi 6
%define GN_rdi 7
%define GN_r8  8
%define GN_r9  9
%define GN_r10 10
%define GN_r11 11
%define GN_r12 12
%define GN_r13 13
%define GN_r14 14
%define GN_r15 15
%assign PRO_T 0
%rep 16
  %assign YN_ymm%[PRO_T] PRO_T
  %assign PRO_T PRO_T+1
%endrep

; callee-saved check: 1 for rbx rbp rsi rdi r12..r15
%define CS_rbx 1
%define CS_rbp 1
%define CS_rsi 1
%define CS_rdi 1
%define CS_r12 1
%define CS_r13 1
%define CS_r14 1
%define CS_r15 1

%macro PROC 3-*
    %xdefine PRO_NAME %1
    %assign  PRO_LOC  (((%2)+15) & ~15)
    %assign  PRO_NXMM %3
    %assign  PRO_NGPR %0-3
    %if PRO_NXMM > 10
        %error "PROC: nxmm must be 0..10"
    %endif
    %assign  PRO_FRAME PRO_LOC + 16*PRO_NXMM + 8*((PRO_NGPR+1) & 1)
    %if PRO_FRAME >= 4096
        %error "PROC: frame of a page or more needs stack probing (__chkstk); not supported"
    %endif
    global %1
    align 16
%1:
    %rotate 3
    %assign PRO_I 0
    %rep PRO_NGPR
        %ifndef CS_%1
            %error "PROC: not a callee-saved GPR:" %1
        %endif
        push    %1
        .pro_g%[PRO_I]:
        %xdefine PRO_GPR%[PRO_I]  %1
        %assign  PRO_GNUM%[PRO_I] GN_%+%1
        %assign  PRO_I PRO_I+1
        %rotate 1
    %endrep
    %if PRO_FRAME > 0
        sub     rsp, PRO_FRAME
    %endif
    .pro_alloc:
    %assign PRO_I 0
    %rep PRO_NXMM
        %assign PRO_J PRO_I+6
        vmovaps [rsp + PRO_LOC + 16*PRO_I], xmm%[PRO_J]
        .pro_x%[PRO_I]:
        %assign PRO_I PRO_I+1
    %endrep
    .pro_end:
%endmacro

%define STK_ARG(n) [rsp + PRO_FRAME + 8*PRO_NGPR + 8*(n)]
%define HOME(n)    [rsp + PRO_FRAME + 8*PRO_NGPR + 8*(n)]

%macro EPILOGUE 0
    %assign PRO_I 0
    %rep PRO_NXMM
        %assign PRO_J PRO_I+6
        vmovaps xmm%[PRO_J], [rsp + PRO_LOC + 16*PRO_I]
        %assign PRO_I PRO_I+1
    %endrep
    %if PRO_FRAME > 0
        add     rsp, PRO_FRAME
    %endif
    %assign PRO_I PRO_NGPR
    %rep PRO_NGPR
        %assign PRO_I PRO_I-1
        pop     PRO_GPR%[PRO_I]
    %endrep
    ret
%endmacro

; Unwind records (x64 exception handling ABI).  Codes are listed in reverse
; order of the prologue.  UWOP_PUSH_NONVOL=0, UWOP_ALLOC_LARGE=1,
; UWOP_ALLOC_SMALL=2, UWOP_SAVE_XMM128=8.
%macro ENDPROC 0
    .pro_fend:
  %ifndef NO_UNWIND               ; -DNO_UNWIND: negative control for the unwind test
    %assign PRO_NCODE PRO_NGPR + 2*PRO_NXMM
    %if PRO_FRAME > 128
        %assign PRO_NCODE PRO_NCODE+2
    %elif PRO_FRAME > 0
        %assign PRO_NCODE PRO_NCODE+1
    %endif
    section .xdata rdata align=8
    align 4, db 0
%%ui:
    db 0x01                         ; version 1, no handler flags
    db .pro_end - PRO_NAME          ; size of prologue in bytes
    db PRO_NCODE                    ; number of unwind code slots
    db 0                            ; no frame register
    %assign PRO_I PRO_NXMM
    %rep PRO_NXMM
        %assign PRO_I PRO_I-1
        %assign PRO_J PRO_I+6
        db .pro_x%[PRO_I] - PRO_NAME
        db (PRO_J << 4) | 8
        dw (PRO_LOC + 16*PRO_I) / 16
    %endrep
    %if PRO_FRAME > 128
        db .pro_alloc - PRO_NAME
        db 0x01
        dw PRO_FRAME / 8
    %elif PRO_FRAME > 0
        db .pro_alloc - PRO_NAME
        db (((PRO_FRAME - 8) / 8) << 4) | 2
    %endif
    %assign PRO_I PRO_NGPR
    %rep PRO_NGPR
        %assign PRO_I PRO_I-1
        db .pro_g%[PRO_I] - PRO_NAME
        db (PRO_GNUM%[PRO_I] << 4) | 0
    %endrep
    %if PRO_NCODE & 1
        dw 0                        ; keep the slot count even
    %endif
    section .pdata rdata align=4
    dd PRO_NAME  wrt ..imagebase
    dd .pro_fend wrt ..imagebase
    dd %%ui      wrt ..imagebase
    section .text
  %endif
%endmacro

; ---- AVX-VNNI in VEX encoding -------------------------------------------
; NASM 2.16.03 knows only the EVEX (AVX-512 VNNI) form of vpdpbusd and
; rejects "{vex} vpdpbusd" ("instruction not encodable with {vex} prefix").
; The EVEX form faults on a CPU without AVX-512, so the VEX form is emitted
; by hand:  VEX.256.66.0F38.W0 50 /r   vpdpbusd ymm1, ymm2, ymm3/m256
;   ymm1 += for each dword: sum of 4 products  u8(ymm2 byte) * s8(ymm3/m256 byte)
;
;   VPDPBUSD_RR  dst, src_u8, src_s8             (three ymm registers)
;   VPDPBUSD_RM  dst, src_u8, base_gpr, disp     (signed bytes at [base + disp])
%macro VEX_0F38_66_256 3-4 0x50     ; dst#, vvvv#, rm#/base#, opcode
    db 0xC4
    db ((((%1)>>3)^1)<<7) | 0x40 | ((((%3)>>3)^1)<<5) | 0x02
    db ((((%2)^15)&15)<<3) | 0x05
    db %4
%endmacro

%macro VPDPBUSD_RR 3
    VEX_0F38_66_256 YN_%+%1, YN_%+%2, YN_%+%3
    db 0xC0 | (((YN_%+%1)&7)<<3) | ((YN_%+%3)&7)
%endmacro

%macro VPDPBUSD_RM 4
    %assign %%b GN_%+%3
    %if (%%b & 7) == 4
        %error "VPDPBUSD_RM: rsp/r12 as base needs a SIB byte; not supported"
    %endif
    VEX_0F38_66_256 YN_%+%1, YN_%+%2, %%b
    %if (%4) == 0 && (%%b & 7) != 5
        db 0x00 | (((YN_%+%1)&7)<<3) | (%%b&7)
    %elif (%4) >= -128 && (%4) <= 127
        db 0x40 | (((YN_%+%1)&7)<<3) | (%%b&7)
        db (%4) & 0xFF
    %else
        db 0x80 | (((YN_%+%1)&7)<<3) | (%%b&7)
        dd %4
    %endif
%endmacro

%endif
