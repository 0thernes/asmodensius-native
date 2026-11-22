; probe_ml64.asm - does MSVC ml64 accept the VEX form?  (ml64 /c probe_ml64.asm)
.code
probe proc
    vex vpdpbusd ymm0, ymm1, ymm2
    vex vpdpbusd ymm0, ymm1, ymmword ptr [rdx+64]
    ret
probe endp
end
