; probe_vex.asm - which spelling of VEX-encoded vpdpbusd does NASM accept?
; Assemble with:  nasm -f win64 -DFORM=n probe_vex.asm -o probe.obj
; FORM 0: plain mnemonic (NASM picks an encoding)
; FORM 1: {vex} prefix
; FORM 2: {vex3} prefix
; FORM 3: {evex} prefix (must NOT be used on this CPU: no AVX-512)
; FORM 4: plain mnemonic with "cpu" directive limiting to no AVX512 (probe)
bits 64
default rel
section .text
%ifndef FORM
%define FORM 0
%endif
%if FORM == 0
    vpdpbusd ymm0, ymm1, ymm2
    vpdpbusd ymm0, ymm1, [rdx+64]
%elif FORM == 1
    {vex} vpdpbusd ymm0, ymm1, ymm2
    {vex} vpdpbusd ymm0, ymm1, [rdx+64]
    {vex} vpdpbusd xmm0, xmm1, xmm2
    {vex} vpdpbusds ymm0, ymm1, ymm2
    {vex} vpdpwssd ymm0, ymm1, ymm2
%elif FORM == 2
    {vex3} vpdpbusd ymm0, ymm1, ymm2
%elif FORM == 3
    {evex} vpdpbusd ymm0, ymm1, ymm2
%endif
