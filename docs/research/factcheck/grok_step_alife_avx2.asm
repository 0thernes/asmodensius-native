; VERBATIM copy of the kernel in GrokBot message 2 (do not edit; this is the exhibit).
; x86-64 AVX2 ALife Neural + Physics Tick Kernel (Windows x64 ABI)
; Processes 8 agents per cycle in 256-bit YMM registers
;
; Arguments (Windows x64 calling convention):
;   RCX = float* pos_x      (64-byte aligned, N floats)
;   RDX = float* vel_x      (64-byte aligned, N floats)
;   R8  = float* sense_in   (64-byte aligned, N floats)
;   R9  = float* weight_w   (64-byte aligned, N floats)
;   [RSP+40] = uint64_t count (multiple of 8, e.g., 100000)

section .rodata
align 32
dt_vec:      times 8 dd 0.016        ; Fixed timestep delta (16ms)
zero_vec:    times 8 dd 0.0          ; ReLU floor
clamp_max:   times 8 dd 5.0          ; Max velocity clamp

section .text
global step_alife_avx2

step_alife_avx2:
    mov     rax, [rsp + 40]          ; Load agent count (N = 100,000)
    xor     r10, r10                 ; Index offset i = 0 (in bytes)

    vbroadcastss ymm13, [rel dt_vec]     ; YMM13 = [0.016 x 8]
    vxorps       ymm14, ymm14, ymm14     ; YMM14 = [0.0 x 8]
    vbroadcastss ymm15, [rel clamp_max]  ; YMM15 = [5.0 x 8]

.loop_8_agents:
    ; 1. Load 8 agents' sensory inputs and synaptic weights directly from L1/L2 cache
    vmovaps ymm0, [r8 + r10]         ; YMM0 = sense_in[i..i+7]
    vmovaps ymm1, [r9 + r10]         ; YMM1 = weight_w[i..i+7]

    ; 2. Neural forward pass: activation = max(0.0, sense * weight)
    vmulps  ymm2, ymm0, ymm1         ; YMM2 = raw weighted input
    vmaxps  ymm2, ymm2, ymm14        ; YMM2 = ReLU(ymm2) (motor thrust)
    vminps  ymm2, ymm2, ymm15        ; Clamp max thrust to 5.0

    ; 3. Integrate velocity & position via Fused Multiply-Add (FMA3)
    ;    pos_x = pos_x + (motor_thrust * dt)
    vmovaps ymm3, [rcx + r10]        ; YMM3 = pos_x[i..i+7]
    vfmadd231ps ymm3, ymm2, ymm13    ; YMM3 += YMM2 * dt (1 single clock cycle!)

    ; 4. Store updated states back to cache
    vmovaps [rcx + r10], ymm3        ; pos_x[i..i+7] = YMM3
    vmovaps [rdx + r10], ymm2        ; vel_x[i..i+7] = YMM2

    add     r10, 32                  ; Advance 8 floats * 4 bytes = 32 bytes
    dec     rax
    jnz     .loop_8_agents

    vzeroupper                       ; Prevent AVX-SSE transition penalty
    ret
