; probe_yasm.asm - does yasm know vpdpbusd at all?  (yasm -f win64 probe_yasm.asm)
bits 64
section .text
    vpdpbusd ymm0, ymm1, ymm2
