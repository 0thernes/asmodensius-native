               	subq	$0x38, %rsp
               	vmovaps	%xmm6, (%rsp)
               	vmovaps	%xmm7, 0x10(%rsp)
               	vmovaps	%xmm8, 0x20(%rsp)
               	vmovdqu	(%r8), %ymm0
               	vmovdqu	0x20(%r8), %ymm1
               	vmovdqu	0x40(%r8), %ymm2
               	vmovdqu	0x60(%r8), %ymm3
               	vpxor	%xmm4, %xmm4, %xmm4
               	vpxor	%xmm5, %xmm5, %xmm5
               	vpxor	%xmm6, %xmm6, %xmm6
               	vpxor	%xmm7, %xmm7, %xmm7
               	vpbroadcastd	(%rcx), %ymm8
               	{vex}	vpdpbusd	(%rdx), %ymm8, %ymm0
               	{vex}	vpdpbusd	0x20(%rdx), %ymm8, %ymm1
               	{vex}	vpdpbusd	0x40(%rdx), %ymm8, %ymm2
               	{vex}	vpdpbusd	0x60(%rdx), %ymm8, %ymm3
               	vpbroadcastd	0x4(%rcx), %ymm8
               	{vex}	vpdpbusd	0x80(%rdx), %ymm8, %ymm4
               	{vex}	vpdpbusd	0xa0(%rdx), %ymm8, %ymm5
               	{vex}	vpdpbusd	0xc0(%rdx), %ymm8, %ymm6
               	{vex}	vpdpbusd	0xe0(%rdx), %ymm8, %ymm7
               	vpbroadcastd	0x8(%rcx), %ymm8
               	{vex}	vpdpbusd	0x100(%rdx), %ymm8, %ymm0
               	{vex}	vpdpbusd	0x120(%rdx), %ymm8, %ymm1
               	{vex}	vpdpbusd	0x140(%rdx), %ymm8, %ymm2
               	{vex}	vpdpbusd	0x160(%rdx), %ymm8, %ymm3
               	vpbroadcastd	0xc(%rcx), %ymm8
               	{vex}	vpdpbusd	0x180(%rdx), %ymm8, %ymm4
               	{vex}	vpdpbusd	0x1a0(%rdx), %ymm8, %ymm5
               	{vex}	vpdpbusd	0x1c0(%rdx), %ymm8, %ymm6
               	{vex}	vpdpbusd	0x1e0(%rdx), %ymm8, %ymm7
               	vpbroadcastd	0x10(%rcx), %ymm8
               	{vex}	vpdpbusd	0x200(%rdx), %ymm8, %ymm0
               	{vex}	vpdpbusd	0x220(%rdx), %ymm8, %ymm1
               	{vex}	vpdpbusd	0x240(%rdx), %ymm8, %ymm2
               	{vex}	vpdpbusd	0x260(%rdx), %ymm8, %ymm3
               	vpbroadcastd	0x14(%rcx), %ymm8
               	{vex}	vpdpbusd	0x280(%rdx), %ymm8, %ymm4
               	{vex}	vpdpbusd	0x2a0(%rdx), %ymm8, %ymm5
               	{vex}	vpdpbusd	0x2c0(%rdx), %ymm8, %ymm6
               	{vex}	vpdpbusd	0x2e0(%rdx), %ymm8, %ymm7
               	vpbroadcastd	0x18(%rcx), %ymm8
               	{vex}	vpdpbusd	0x300(%rdx), %ymm8, %ymm0
               	{vex}	vpdpbusd	0x320(%rdx), %ymm8, %ymm1
               	{vex}	vpdpbusd	0x340(%rdx), %ymm8, %ymm2
               	{vex}	vpdpbusd	0x360(%rdx), %ymm8, %ymm3
               	vpbroadcastd	0x1c(%rcx), %ymm8
               	{vex}	vpdpbusd	0x380(%rdx), %ymm8, %ymm4
               	{vex}	vpdpbusd	0x3a0(%rdx), %ymm8, %ymm5
               	{vex}	vpdpbusd	0x3c0(%rdx), %ymm8, %ymm6
               	{vex}	vpdpbusd	0x3e0(%rdx), %ymm8, %ymm7
               	vpbroadcastd	0x20(%rcx), %ymm8
               	{vex}	vpdpbusd	0x400(%rdx), %ymm8, %ymm0
               	{vex}	vpdpbusd	0x420(%rdx), %ymm8, %ymm1
               	{vex}	vpdpbusd	0x440(%rdx), %ymm8, %ymm2
               	{vex}	vpdpbusd	0x460(%rdx), %ymm8, %ymm3
               	vpbroadcastd	0x24(%rcx), %ymm8
               	{vex}	vpdpbusd	0x480(%rdx), %ymm8, %ymm4
               	{vex}	vpdpbusd	0x4a0(%rdx), %ymm8, %ymm5
               	{vex}	vpdpbusd	0x4c0(%rdx), %ymm8, %ymm6
               	{vex}	vpdpbusd	0x4e0(%rdx), %ymm8, %ymm7
               	vpbroadcastd	0x28(%rcx), %ymm8
               	{vex}	vpdpbusd	0x500(%rdx), %ymm8, %ymm0
               	{vex}	vpdpbusd	0x520(%rdx), %ymm8, %ymm1
               	{vex}	vpdpbusd	0x540(%rdx), %ymm8, %ymm2
               	{vex}	vpdpbusd	0x560(%rdx), %ymm8, %ymm3
               	vpbroadcastd	0x2c(%rcx), %ymm8
               	{vex}	vpdpbusd	0x580(%rdx), %ymm8, %ymm4
               	{vex}	vpdpbusd	0x5a0(%rdx), %ymm8, %ymm5
               	{vex}	vpdpbusd	0x5c0(%rdx), %ymm8, %ymm6
               	{vex}	vpdpbusd	0x5e0(%rdx), %ymm8, %ymm7
               	vpbroadcastd	0x30(%rcx), %ymm8
               	{vex}	vpdpbusd	0x600(%rdx), %ymm8, %ymm0
               	{vex}	vpdpbusd	0x620(%rdx), %ymm8, %ymm1
               	{vex}	vpdpbusd	0x640(%rdx), %ymm8, %ymm2
               	{vex}	vpdpbusd	0x660(%rdx), %ymm8, %ymm3
               	vpbroadcastd	0x34(%rcx), %ymm8
               	{vex}	vpdpbusd	0x680(%rdx), %ymm8, %ymm4
               	{vex}	vpdpbusd	0x6a0(%rdx), %ymm8, %ymm5
               	{vex}	vpdpbusd	0x6c0(%rdx), %ymm8, %ymm6
               	{vex}	vpdpbusd	0x6e0(%rdx), %ymm8, %ymm7
               	vpbroadcastd	0x38(%rcx), %ymm8
               	{vex}	vpdpbusd	0x700(%rdx), %ymm8, %ymm0
               	{vex}	vpdpbusd	0x720(%rdx), %ymm8, %ymm1
               	{vex}	vpdpbusd	0x740(%rdx), %ymm8, %ymm2
               	{vex}	vpdpbusd	0x760(%rdx), %ymm8, %ymm3
               	vpbroadcastd	0x3c(%rcx), %ymm8
               	{vex}	vpdpbusd	0x780(%rdx), %ymm8, %ymm4
               	{vex}	vpdpbusd	0x7a0(%rdx), %ymm8, %ymm5
               	{vex}	vpdpbusd	0x7c0(%rdx), %ymm8, %ymm6
               	{vex}	vpdpbusd	0x7e0(%rdx), %ymm8, %ymm7
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpaddd	%ymm5, %ymm1, %ymm1
               	vpaddd	%ymm6, %ymm2, %ymm2
               	vpaddd	%ymm7, %ymm3, %ymm3
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
               	movq	0x60(%rsp), %rax
               	vmovdqu	%ymm0, (%rax)
               	vzeroupper
               	vmovaps	(%rsp), %xmm6
               	vmovaps	0x10(%rsp), %xmm7
               	vmovaps	0x20(%rsp), %xmm8
               	addq	$0x38, %rsp
