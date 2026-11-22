               	subq	$0x88, %rsp
               	vmovaps	%xmm6, (%rsp)
               	vmovaps	%xmm7, 0x10(%rsp)
               	vmovaps	%xmm8, 0x20(%rsp)
               	vmovaps	%xmm9, 0x30(%rsp)
               	vmovaps	%xmm10, 0x40(%rsp)
               	vmovaps	%xmm11, 0x50(%rsp)
               	vmovaps	%xmm12, 0x60(%rsp)
               	vmovaps	%xmm13, 0x70(%rsp)
               	vmovdqu	(%rcx), %ymm8
               	vmovdqu	0x20(%rcx), %ymm9
               	vpxor	%xmm0, %xmm0, %xmm0
               	{vex}	vpdpbusd	(%rdx), %ymm8, %ymm0
               	{vex}	vpdpbusd	0x20(%rdx), %ymm9, %ymm0
               	vpxor	%xmm1, %xmm1, %xmm1
               	{vex}	vpdpbusd	0x40(%rdx), %ymm8, %ymm1
               	{vex}	vpdpbusd	0x60(%rdx), %ymm9, %ymm1
               	vpxor	%xmm2, %xmm2, %xmm2
               	{vex}	vpdpbusd	0x80(%rdx), %ymm8, %ymm2
               	{vex}	vpdpbusd	0xa0(%rdx), %ymm9, %ymm2
               	vpxor	%xmm3, %xmm3, %xmm3
               	{vex}	vpdpbusd	0xc0(%rdx), %ymm8, %ymm3
               	{vex}	vpdpbusd	0xe0(%rdx), %ymm9, %ymm3
               	vpxor	%xmm4, %xmm4, %xmm4
               	{vex}	vpdpbusd	0x100(%rdx), %ymm8, %ymm4
               	{vex}	vpdpbusd	0x120(%rdx), %ymm9, %ymm4
               	vpxor	%xmm5, %xmm5, %xmm5
               	{vex}	vpdpbusd	0x140(%rdx), %ymm8, %ymm5
               	{vex}	vpdpbusd	0x160(%rdx), %ymm9, %ymm5
               	vpxor	%xmm6, %xmm6, %xmm6
               	{vex}	vpdpbusd	0x180(%rdx), %ymm8, %ymm6
               	{vex}	vpdpbusd	0x1a0(%rdx), %ymm9, %ymm6
               	vpxor	%xmm7, %xmm7, %xmm7
               	{vex}	vpdpbusd	0x1c0(%rdx), %ymm8, %ymm7
               	{vex}	vpdpbusd	0x1e0(%rdx), %ymm9, %ymm7
               	vphaddd	%ymm1, %ymm0, %ymm0
               	vphaddd	%ymm3, %ymm2, %ymm2
               	vphaddd	%ymm5, %ymm4, %ymm4
               	vphaddd	%ymm7, %ymm6, %ymm6
               	vphaddd	%ymm2, %ymm0, %ymm0
               	vphaddd	%ymm6, %ymm4, %ymm4
               	vextracti128	$0x1, %ymm0, %xmm1
               	vpaddd	%xmm1, %xmm0, %xmm0
               	vextracti128	$0x1, %ymm4, %xmm5
               	vpaddd	%xmm5, %xmm4, %xmm4
               	vinserti128	$0x1, %xmm4, %ymm0, %ymm10
               	vpaddd	(%r8), %ymm10, %ymm10
               	vpxor	%xmm0, %xmm0, %xmm0
               	{vex}	vpdpbusd	0x200(%rdx), %ymm8, %ymm0
               	{vex}	vpdpbusd	0x220(%rdx), %ymm9, %ymm0
               	vpxor	%xmm1, %xmm1, %xmm1
               	{vex}	vpdpbusd	0x240(%rdx), %ymm8, %ymm1
               	{vex}	vpdpbusd	0x260(%rdx), %ymm9, %ymm1
               	vpxor	%xmm2, %xmm2, %xmm2
               	{vex}	vpdpbusd	0x280(%rdx), %ymm8, %ymm2
               	{vex}	vpdpbusd	0x2a0(%rdx), %ymm9, %ymm2
               	vpxor	%xmm3, %xmm3, %xmm3
               	{vex}	vpdpbusd	0x2c0(%rdx), %ymm8, %ymm3
               	{vex}	vpdpbusd	0x2e0(%rdx), %ymm9, %ymm3
               	vpxor	%xmm4, %xmm4, %xmm4
               	{vex}	vpdpbusd	0x300(%rdx), %ymm8, %ymm4
               	{vex}	vpdpbusd	0x320(%rdx), %ymm9, %ymm4
               	vpxor	%xmm5, %xmm5, %xmm5
               	{vex}	vpdpbusd	0x340(%rdx), %ymm8, %ymm5
               	{vex}	vpdpbusd	0x360(%rdx), %ymm9, %ymm5
               	vpxor	%xmm6, %xmm6, %xmm6
               	{vex}	vpdpbusd	0x380(%rdx), %ymm8, %ymm6
               	{vex}	vpdpbusd	0x3a0(%rdx), %ymm9, %ymm6
               	vpxor	%xmm7, %xmm7, %xmm7
               	{vex}	vpdpbusd	0x3c0(%rdx), %ymm8, %ymm7
               	{vex}	vpdpbusd	0x3e0(%rdx), %ymm9, %ymm7
               	vphaddd	%ymm1, %ymm0, %ymm0
               	vphaddd	%ymm3, %ymm2, %ymm2
               	vphaddd	%ymm5, %ymm4, %ymm4
               	vphaddd	%ymm7, %ymm6, %ymm6
               	vphaddd	%ymm2, %ymm0, %ymm0
               	vphaddd	%ymm6, %ymm4, %ymm4
               	vextracti128	$0x1, %ymm0, %xmm1
               	vpaddd	%xmm1, %xmm0, %xmm0
               	vextracti128	$0x1, %ymm4, %xmm5
               	vpaddd	%xmm5, %xmm4, %xmm4
               	vinserti128	$0x1, %xmm4, %ymm0, %ymm11
               	vpaddd	0x20(%r8), %ymm11, %ymm11
               	vpxor	%xmm0, %xmm0, %xmm0
               	{vex}	vpdpbusd	0x400(%rdx), %ymm8, %ymm0
               	{vex}	vpdpbusd	0x420(%rdx), %ymm9, %ymm0
               	vpxor	%xmm1, %xmm1, %xmm1
               	{vex}	vpdpbusd	0x440(%rdx), %ymm8, %ymm1
               	{vex}	vpdpbusd	0x460(%rdx), %ymm9, %ymm1
               	vpxor	%xmm2, %xmm2, %xmm2
               	{vex}	vpdpbusd	0x480(%rdx), %ymm8, %ymm2
               	{vex}	vpdpbusd	0x4a0(%rdx), %ymm9, %ymm2
               	vpxor	%xmm3, %xmm3, %xmm3
               	{vex}	vpdpbusd	0x4c0(%rdx), %ymm8, %ymm3
               	{vex}	vpdpbusd	0x4e0(%rdx), %ymm9, %ymm3
               	vpxor	%xmm4, %xmm4, %xmm4
               	{vex}	vpdpbusd	0x500(%rdx), %ymm8, %ymm4
               	{vex}	vpdpbusd	0x520(%rdx), %ymm9, %ymm4
               	vpxor	%xmm5, %xmm5, %xmm5
               	{vex}	vpdpbusd	0x540(%rdx), %ymm8, %ymm5
               	{vex}	vpdpbusd	0x560(%rdx), %ymm9, %ymm5
               	vpxor	%xmm6, %xmm6, %xmm6
               	{vex}	vpdpbusd	0x580(%rdx), %ymm8, %ymm6
               	{vex}	vpdpbusd	0x5a0(%rdx), %ymm9, %ymm6
               	vpxor	%xmm7, %xmm7, %xmm7
               	{vex}	vpdpbusd	0x5c0(%rdx), %ymm8, %ymm7
               	{vex}	vpdpbusd	0x5e0(%rdx), %ymm9, %ymm7
               	vphaddd	%ymm1, %ymm0, %ymm0
               	vphaddd	%ymm3, %ymm2, %ymm2
               	vphaddd	%ymm5, %ymm4, %ymm4
               	vphaddd	%ymm7, %ymm6, %ymm6
               	vphaddd	%ymm2, %ymm0, %ymm0
               	vphaddd	%ymm6, %ymm4, %ymm4
               	vextracti128	$0x1, %ymm0, %xmm1
               	vpaddd	%xmm1, %xmm0, %xmm0
               	vextracti128	$0x1, %ymm4, %xmm5
               	vpaddd	%xmm5, %xmm4, %xmm4
               	vinserti128	$0x1, %xmm4, %ymm0, %ymm12
               	vpaddd	0x40(%r8), %ymm12, %ymm12
               	vpxor	%xmm0, %xmm0, %xmm0
               	{vex}	vpdpbusd	0x600(%rdx), %ymm8, %ymm0
               	{vex}	vpdpbusd	0x620(%rdx), %ymm9, %ymm0
               	vpxor	%xmm1, %xmm1, %xmm1
               	{vex}	vpdpbusd	0x640(%rdx), %ymm8, %ymm1
               	{vex}	vpdpbusd	0x660(%rdx), %ymm9, %ymm1
               	vpxor	%xmm2, %xmm2, %xmm2
               	{vex}	vpdpbusd	0x680(%rdx), %ymm8, %ymm2
               	{vex}	vpdpbusd	0x6a0(%rdx), %ymm9, %ymm2
               	vpxor	%xmm3, %xmm3, %xmm3
               	{vex}	vpdpbusd	0x6c0(%rdx), %ymm8, %ymm3
               	{vex}	vpdpbusd	0x6e0(%rdx), %ymm9, %ymm3
               	vpxor	%xmm4, %xmm4, %xmm4
               	{vex}	vpdpbusd	0x700(%rdx), %ymm8, %ymm4
               	{vex}	vpdpbusd	0x720(%rdx), %ymm9, %ymm4
               	vpxor	%xmm5, %xmm5, %xmm5
               	{vex}	vpdpbusd	0x740(%rdx), %ymm8, %ymm5
               	{vex}	vpdpbusd	0x760(%rdx), %ymm9, %ymm5
               	vpxor	%xmm6, %xmm6, %xmm6
               	{vex}	vpdpbusd	0x780(%rdx), %ymm8, %ymm6
               	{vex}	vpdpbusd	0x7a0(%rdx), %ymm9, %ymm6
               	vpxor	%xmm7, %xmm7, %xmm7
               	{vex}	vpdpbusd	0x7c0(%rdx), %ymm8, %ymm7
               	{vex}	vpdpbusd	0x7e0(%rdx), %ymm9, %ymm7
               	vphaddd	%ymm1, %ymm0, %ymm0
               	vphaddd	%ymm3, %ymm2, %ymm2
               	vphaddd	%ymm5, %ymm4, %ymm4
               	vphaddd	%ymm7, %ymm6, %ymm6
               	vphaddd	%ymm2, %ymm0, %ymm0
               	vphaddd	%ymm6, %ymm4, %ymm4
               	vextracti128	$0x1, %ymm0, %xmm1
               	vpaddd	%xmm1, %xmm0, %xmm0
               	vextracti128	$0x1, %ymm4, %xmm5
               	vpaddd	%xmm5, %xmm4, %xmm4
               	vinserti128	$0x1, %xmm4, %ymm0, %ymm13
               	vpaddd	0x60(%r8), %ymm13, %ymm13
               	vmovdqa	%ymm10, %ymm0
               	vmovdqa	%ymm11, %ymm1
               	vmovdqa	%ymm12, %ymm2
               	vmovdqa	%ymm13, %ymm3
               	vmovd	%r9d, %xmm4
               	vpsrad	%xmm4, %ymm0, %ymm0
               	vpsrad	%xmm4, %ymm1, %ymm1
               	vpsrad	%xmm4, %ymm2, %ymm2
               	vpsrad	%xmm4, %ymm3, %ymm3
               	vpackssdw	%ymm1, %ymm0, %ymm0
               	vpackssdw	%ymm3, %ymm2, %ymm2
               	vmovdqa	(%rip), %ymm5           
               	vpminsw	%ymm5, %ymm0, %ymm0
               	vpminsw	%ymm5, %ymm2, %ymm2
               	vpackuswb	%ymm2, %ymm0, %ymm0
               	vmovdqa	0x20(%rip), %ymm4       
               	vpermd	%ymm0, %ymm4, %ymm0
               	movq	0xb0(%rsp), %rax
               	vmovdqu	%ymm0, (%rax)
               	vzeroupper
               	vmovaps	(%rsp), %xmm6
               	vmovaps	0x10(%rsp), %xmm7
               	vmovaps	0x20(%rsp), %xmm8
               	vmovaps	0x30(%rsp), %xmm9
               	vmovaps	0x40(%rsp), %xmm10
               	vmovaps	0x50(%rsp), %xmm11
               	vmovaps	0x60(%rsp), %xmm12
               	vmovaps	0x70(%rsp), %xmm13
               	addq	$0x88, %rsp
