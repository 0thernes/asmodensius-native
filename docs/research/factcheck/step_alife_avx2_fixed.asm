; step_alife_avx2_fixed.asm
; Corrected version of the exhibit grok_step_alife_avx2.asm (same arithmetic,
; same 8-wide AVX2 + FMA loop body). Written for the fact-check in asm_kernel.md.
;
; What was wrong in the exhibit and what changed here:
;   1. Loop counter. The exhibit loads the AGENT count into RAX and decrements
;      it once per 8-agent block, so it runs 8 times too far (count=100000
;      processes 800000 agents and writes 7 x 400 KB past the end of pos_x and
;      vel_x). Here RAX = count / 8 blocks.
;   2. Windows x64 ABI. XMM6..XMM15 are callee-saved; the exhibit overwrites
;      YMM13, YMM14, YMM15 and never restores them. Here only the volatile
;      registers XMM0..XMM5 are used, so nothing has to be saved and the
;      function stays a legal leaf function (no stack frame, no unwind data).
;   3. count = 0. The exhibit's dec/jnz would wrap and run 2^64 times. Here a
;      zero block count returns at once.
;   4. Constants. The exhibit puts them in "section .rodata", which NASM's
;      win64 output does not know and therefore marks as CODE (executable).
;      Here they are in .rdata (read-only data).
;
; What it still is: one multiply, a ReLU and a clamp per agent, then a
; position update. There is no tanh, no dot product over several inputs and
; no neighbour interaction (see asm_kernel.md, "Section F").
;
; uint64_t step_alife_avx2_fixed(float *pos_x,          ; RCX, 32-byte aligned
;                                float *vel_x,          ; RDX, 32-byte aligned
;                                const float *sense_in, ; R8,  32-byte aligned
;                                const float *weight_w, ; R9,  32-byte aligned
;                                uint64_t count);       ; [RSP+40] at entry
; Precondition: count is a multiple of 8 (a remainder of 1..7 agents is left
; untouched). Returns the number of agents processed.
;
; Build: nasm -f win64 step_alife_avx2_fixed.asm -o fixed.obj

default rel

section .rdata
align 4
dt_val:     dd 0.016                 ; fixed timestep
clamp_val:  dd 5.0                   ; thrust clamp

section .text
global step_alife_avx2_fixed

step_alife_avx2_fixed:
    mov     rax, [rsp + 40]          ; fifth argument: agent count
    shr     rax, 3                   ; blocks of 8 agents
    jz      .done                    ; nothing to do (RAX = 0 = agents processed)
    mov     r11, rax
    shl     r11, 3                   ; agents that will be processed
    xor     r10d, r10d               ; byte offset

    vbroadcastss ymm4, [dt_val]      ; YMM4 = dt        (volatile register)
    vxorps       xmm5, xmm5, xmm5    ; YMM5 = 0.0       (volatile register)
    vbroadcastss ymm1, [clamp_val]   ; YMM1 = 5.0       (volatile register)

align 16
.loop:
    vmovaps ymm0, [r8 + r10]         ; sense_in[i..i+7]
    vmulps  ymm2, ymm0, [r9 + r10]   ; sense * weight
    vmaxps  ymm2, ymm2, ymm5         ; ReLU
    vminps  ymm2, ymm2, ymm1         ; clamp to 5.0  -> thrust
    vmovaps ymm3, [rcx + r10]        ; pos_x[i..i+7]
    vfmadd231ps ymm3, ymm2, ymm4     ; pos += thrust * dt
    vmovaps [rcx + r10], ymm3        ; store pos_x
    vmovaps [rdx + r10], ymm2        ; store vel_x (= thrust)
    add     r10, 32
    dec     rax
    jnz     .loop

    vzeroupper
    mov     rax, r11                 ; agents processed
.done:
    ret
