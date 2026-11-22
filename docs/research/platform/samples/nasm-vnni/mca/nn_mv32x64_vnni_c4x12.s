               	subq	$0x78, %rsp
               	vmovaps	%xmm6, (%rsp)
               	vmovaps	%xmm7, 0x10(%rsp)
               	vmovaps	%xmm8, 0x20(%rsp)
               	vmovaps	%xmm9, 0x30(%rsp)
               	vmovaps	%xmm10, 0x40(%rsp)
               	vmovaps	%xmm11, 0x50(%rsp)
               	vmovaps	%xmm12, 0x60(%rsp)
               	vmovdqu	(%r8), %ymm0
               	vmovdqu	0x20(%r8), %ymm1
               	vmovdqu	0x40(%r8), %ymm2
               	vmovdqu	0x60(%r8), %ymm3
               	vpxor	%xmm4, %xmm4, %xmm4
               	vpxor	%xmm5, %xmm5, %xmm5
               	vpxor	%xmm6, %xmm6, %xmm6
               	vpxor	%xmm7, %xmm7, %xmm7
               	vpxor	%xmm8, %xmm8, %xmm8
               	vpxor	%xmm9, %xmm9, %xmm9
               	vpxor	%xmm10, %xmm10, %xmm10
               	vpxor	%xmm11, %xmm11, %xmm11
               	vpbroadcastd	(%rcx), %ymm12
               	{vex}	vpdpbusd	(%rdx), %ymm12, %ymm0
               	{vex}	vpdpbusd	0x20(%rdx), %ymm12, %ymm1
               	{vex}	vpdpbusd	0x40(%rdx), %ymm12, %ymm2
               	{vex}	vpdpbusd	0x60(%rdx), %ymm12, %ymm3
               	vpbroadcastd	0x4(%rcx), %ymm12
               	{vex}	vpdpbusd	0x80(%rdx), %ymm12, %ymm4
               	{vex}	vpdpbusd	0xa0(%rdx), %ymm12, %ymm5
               	{vex}	vpdpbusd	0xc0(%rdx), %ymm12, %ymm6
               	{vex}	vpdpbusd	0xe0(%rdx), %ymm12, %ymm7
               	vpbroadcastd	0x8(%rcx), %ymm12
               	{vex}	vpdpbusd	0x100(%rdx), %ymm12, %ymm8
               	{vex}	vpdpbusd	0x120(%rdx), %ymm12, %ymm9
               	{vex}	vpdpbusd	0x140(%rdx), %ymm12, %ymm10
               	{vex}	vpdpbusd	0x160(%rdx), %ymm12, %ymm11
               	vpbroadcastd	0xc(%rcx), %ymm12
               	{vex}	vpdpbusd	0x180(%rdx), %ymm12, %ymm0
               	{vex}	vpdpbusd	0x1a0(%rdx), %ymm12, %ymm1
               	{vex}	vpdpbusd	0x1c0(%rdx), %ymm12, %ymm2
               	{vex}	vpdpbusd	0x1e0(%rdx), %ymm12, %ymm3
               	vpbroadcastd	0x10(%rcx), %ymm12
               	{vex}	vpdpbusd	0x200(%rdx), %ymm12, %ymm4
               	{vex}	vpdpbusd	0x220(%rdx), %ymm12, %ymm5
               	{vex}	vpdpbusd	0x240(%rdx), %ymm12, %ymm6
               	{vex}	vpdpbusd	0x260(%rdx), %ymm12, %ymm7
               	vpbroadcastd	0x14(%rcx), %ymm12
               	{vex}	vpdpbusd	0x280(%rdx), %ymm12, %ymm8
               	{vex}	vpdpbusd	0x2a0(%rdx), %ymm12, %ymm9
               	{vex}	vpdpbusd	0x2c0(%rdx), %ymm12, %ymm10
               	{vex}	vpdpbusd	0x2e0(%rdx), %ymm12, %ymm11
               	vpbroadcastd	0x18(%rcx), %ymm12
               	{vex}	vpdpbusd	0x300(%rdx), %ymm12, %ymm0
               	{vex}	vpdpbusd	0x320(%rdx), %ymm12, %ymm1
               	{vex}	vpdpbusd	0x340(%rdx), %ymm12, %ymm2
               	{vex}	vpdpbusd	0x360(%rdx), %ymm12, %ymm3
               	vpbroadcastd	0x1c(%rcx), %ymm12
               	{vex}	vpdpbusd	0x380(%rdx), %ymm12, %ymm4
               	{vex}	vpdpbusd	0x3a0(%rdx), %ymm12, %ymm5
               	{vex}	vpdpbusd	0x3c0(%rdx), %ymm12, %ymm6
               	{vex}	vpdpbusd	0x3e0(%rdx), %ymm12, %ymm7
               	vpbroadcastd	0x20(%rcx), %ymm12
               	{vex}	vpdpbusd	0x400(%rdx), %ymm12, %ymm8
               	{vex}	vpdpbusd	0x420(%rdx), %ymm12, %ymm9
               	{vex}	vpdpbusd	0x440(%rdx), %ymm12, %ymm10
               	{vex}	vpdpbusd	0x460(%rdx), %ymm12, %ymm11
               	vpbroadcastd	0x24(%rcx), %ymm12
               	{vex}	vpdpbusd	0x480(%rdx), %ymm12, %ymm0
               	{vex}	vpdpbusd	0x4a0(%rdx), %ymm12, %ymm1
               	{vex}	vpdpbusd	0x4c0(%rdx), %ymm12, %ymm2
               	{vex}	vpdpbusd	0x4e0(%rdx), %ymm12, %ymm3
               	vpbroadcastd	0x28(%rcx), %ymm12
               	{vex}	vpdpbusd	0x500(%rdx), %ymm12, %ymm4
               	{vex}	vpdpbusd	0x520(%rdx), %ymm12, %ymm5
               	{vex}	vpdpbusd	0x540(%rdx), %ymm12, %ymm6
               	{vex}	vpdpbusd	0x560(%rdx), %ymm12, %ymm7
               	vpbroadcastd	0x2c(%rcx), %ymm12
               	{vex}	vpdpbusd	0x580(%rdx), %ymm12, %ymm8
               	{vex}	vpdpbusd	0x5a0(%rdx), %ymm12, %ymm9
               	{vex}	vpdpbusd	0x5c0(%rdx), %ymm12, %ymm10
               	{vex}	vpdpbusd	0x5e0(%rdx), %ymm12, %ymm11
               	vpbroadcastd	0x30(%rcx), %ymm12
               	{vex}	vpdpbusd	0x600(%rdx), %ymm12, %ymm0
               	{vex}	vpdpbusd	0x620(%rdx), %ymm12, %ymm1
               	{vex}	vpdpbusd	0x640(%rdx), %ymm12, %ymm2
               	{vex}	vpdpbusd	0x660(%rdx), %ymm12, %ymm3
               	vpbroadcastd	0x34(%rcx), %ymm12
               	{vex}	vpdpbusd	0x680(%rdx), %ymm12, %ymm4
               	{vex}	vpdpbusd	0x6a0(%rdx), %ymm12, %ymm5
               	{vex}	vpdpbusd	0x6c0(%rdx), %ymm12, %ymm6
               	{vex}	vpdpbusd	0x6e0(%rdx), %ymm12, %ymm7
               	vpbroadcastd	0x38(%rcx), %ymm12
               	{vex}	vpdpbusd	0x700(%rdx), %ymm12, %ymm8
               	{vex}	vpdpbusd	0x720(%rdx), %ymm12, %ymm9
               	{vex}	vpdpbusd	0x740(%rdx), %ymm12, %ymm10
               	{vex}	vpdpbusd	0x760(%rdx), %ymm12, %ymm11
               	vpbroadcastd	0x3c(%rcx), %ymm12
               	{vex}	vpdpbusd	0x780(%rdx), %ymm12, %ymm0
               	{vex}	vpdpbusd	0x7a0(%rdx), %ymm12, %ymm1
               	{vex}	vpdpbusd	0x7c0(%rdx), %ymm12, %ymm2
               	{vex}	vpdpbusd	0x7e0(%rdx), %ymm12, %ymm3
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpaddd	%ymm5, %ymm1, %ymm1
               	vpaddd	%ymm6, %ymm2, %ymm2
               	vpaddd	%ymm7, %ymm3, %ymm3
               	vpaddd	%ymm8, %ymm0, %ymm0
               	vpaddd	%ymm9, %ymm1, %ymm1
               	vpaddd	%ymm10, %ymm2, %ymm2
               	vpaddd	%ymm11, %ymm3, %ymm3
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
               	movq	0xa0(%rsp), %rax
               	vmovdqu	%ymm0, (%rax)
               	vzeroupper
               	vmovaps	(%rsp), %xmm6
               	vmovaps	0x10(%rsp), %xmm7
               	vmovaps	0x20(%rsp), %xmm8
               	vmovaps	0x30(%rsp), %xmm9
               	vmovaps	0x40(%rsp), %xmm10
               	vmovaps	0x50(%rsp), %xmm11
               	vmovaps	0x60(%rsp), %xmm12
               	addq	$0x78, %rsp
