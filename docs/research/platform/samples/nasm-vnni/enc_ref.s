# enc_ref.s - the same instructions as enc_check.asm, assembled by clang's
# integrated assembler with the {vex} prefix.  The .text bytes of the two
# objects must be identical.
    .intel_syntax noprefix
    .text
    {vex} vpdpbusd ymm0,  ymm1,  ymm2
    {vex} vpdpbusd ymm7,  ymm8,  ymm15
    {vex} vpdpbusd ymm8,  ymm0,  ymm7
    {vex} vpdpbusd ymm15, ymm15, ymm15
    {vex} vpdpbusd ymm12, ymm3,  ymm9
    {vex} vpdpbusd ymm0,  ymm1,  ymmword ptr [rdx]
    {vex} vpdpbusd ymm3,  ymm12, ymmword ptr [rdx + 32]
    {vex} vpdpbusd ymm3,  ymm12, ymmword ptr [rdx + 127]
    {vex} vpdpbusd ymm3,  ymm12, ymmword ptr [rdx + 128]
    {vex} vpdpbusd ymm11, ymm8,  ymmword ptr [rdx + 2016]
    {vex} vpdpbusd ymm9,  ymm4,  ymmword ptr [rcx - 64]
    {vex} vpdpbusd ymm9,  ymm4,  ymmword ptr [rcx - 129]
    {vex} vpdpbusd ymm2,  ymm5,  ymmword ptr [rbp]
    {vex} vpdpbusd ymm2,  ymm5,  ymmword ptr [r13]
    {vex} vpdpbusd ymm14, ymm6,  ymmword ptr [r8]
    {vex} vpdpbusd ymm14, ymm6,  ymmword ptr [r15 + 4096]
    {vex} vpdpbusd ymm1,  ymm2,  ymmword ptr [rax + 8]
    {vex} vpdpbusd ymm1,  ymm2,  ymmword ptr [rsi + 8]
    {vex} vpdpbusd ymm1,  ymm2,  ymmword ptr [rdi + 8]
    {vex} vpdpbusd ymm1,  ymm2,  ymmword ptr [rbx + 8]
    {vex} vpdpbusd ymm1,  ymm2,  ymmword ptr [r9 + 8]
    {vex} vpdpbusd ymm1,  ymm2,  ymmword ptr [r10 + 8]
    {vex} vpdpbusd ymm1,  ymm2,  ymmword ptr [r11 + 8]
    {vex} vpdpbusd ymm1,  ymm2,  ymmword ptr [r14 + 8]
