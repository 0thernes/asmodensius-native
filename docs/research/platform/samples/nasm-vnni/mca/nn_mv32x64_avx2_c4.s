               	subq	$0x18, %rsp
               	vmovaps	%xmm6, (%rsp)
               	vmovdqu	(%r8), %ymm0
               	vmovdqu	0x20(%r8), %ymm1
               	vmovdqu	0x40(%r8), %ymm2
               	vmovdqu	0x60(%r8), %ymm3
               	vpcmpeqw	%ymm6, %ymm6, %ymm6
               	vpsrlw	$0xf, %ymm6, %ymm6
               	vpbroadcastd	(%rcx), %ymm5
               	vpmaddubsw	(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpmaddubsw	0x20(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm1, %ymm1
               	vpmaddubsw	0x40(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm2, %ymm2
               	vpmaddubsw	0x60(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm3, %ymm3
               	vpbroadcastd	0x4(%rcx), %ymm5
               	vpmaddubsw	0x80(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpmaddubsw	0xa0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm1, %ymm1
               	vpmaddubsw	0xc0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm2, %ymm2
               	vpmaddubsw	0xe0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm3, %ymm3
               	vpbroadcastd	0x8(%rcx), %ymm5
               	vpmaddubsw	0x100(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpmaddubsw	0x120(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm1, %ymm1
               	vpmaddubsw	0x140(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm2, %ymm2
               	vpmaddubsw	0x160(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm3, %ymm3
               	vpbroadcastd	0xc(%rcx), %ymm5
               	vpmaddubsw	0x180(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpmaddubsw	0x1a0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm1, %ymm1
               	vpmaddubsw	0x1c0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm2, %ymm2
               	vpmaddubsw	0x1e0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm3, %ymm3
               	vpbroadcastd	0x10(%rcx), %ymm5
               	vpmaddubsw	0x200(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpmaddubsw	0x220(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm1, %ymm1
               	vpmaddubsw	0x240(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm2, %ymm2
               	vpmaddubsw	0x260(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm3, %ymm3
               	vpbroadcastd	0x14(%rcx), %ymm5
               	vpmaddubsw	0x280(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpmaddubsw	0x2a0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm1, %ymm1
               	vpmaddubsw	0x2c0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm2, %ymm2
               	vpmaddubsw	0x2e0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm3, %ymm3
               	vpbroadcastd	0x18(%rcx), %ymm5
               	vpmaddubsw	0x300(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpmaddubsw	0x320(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm1, %ymm1
               	vpmaddubsw	0x340(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm2, %ymm2
               	vpmaddubsw	0x360(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm3, %ymm3
               	vpbroadcastd	0x1c(%rcx), %ymm5
               	vpmaddubsw	0x380(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpmaddubsw	0x3a0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm1, %ymm1
               	vpmaddubsw	0x3c0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm2, %ymm2
               	vpmaddubsw	0x3e0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm3, %ymm3
               	vpbroadcastd	0x20(%rcx), %ymm5
               	vpmaddubsw	0x400(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpmaddubsw	0x420(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm1, %ymm1
               	vpmaddubsw	0x440(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm2, %ymm2
               	vpmaddubsw	0x460(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm3, %ymm3
               	vpbroadcastd	0x24(%rcx), %ymm5
               	vpmaddubsw	0x480(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpmaddubsw	0x4a0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm1, %ymm1
               	vpmaddubsw	0x4c0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm2, %ymm2
               	vpmaddubsw	0x4e0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm3, %ymm3
               	vpbroadcastd	0x28(%rcx), %ymm5
               	vpmaddubsw	0x500(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpmaddubsw	0x520(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm1, %ymm1
               	vpmaddubsw	0x540(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm2, %ymm2
               	vpmaddubsw	0x560(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm3, %ymm3
               	vpbroadcastd	0x2c(%rcx), %ymm5
               	vpmaddubsw	0x580(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpmaddubsw	0x5a0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm1, %ymm1
               	vpmaddubsw	0x5c0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm2, %ymm2
               	vpmaddubsw	0x5e0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm3, %ymm3
               	vpbroadcastd	0x30(%rcx), %ymm5
               	vpmaddubsw	0x600(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpmaddubsw	0x620(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm1, %ymm1
               	vpmaddubsw	0x640(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm2, %ymm2
               	vpmaddubsw	0x660(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm3, %ymm3
               	vpbroadcastd	0x34(%rcx), %ymm5
               	vpmaddubsw	0x680(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpmaddubsw	0x6a0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm1, %ymm1
               	vpmaddubsw	0x6c0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm2, %ymm2
               	vpmaddubsw	0x6e0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm3, %ymm3
               	vpbroadcastd	0x38(%rcx), %ymm5
               	vpmaddubsw	0x700(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpmaddubsw	0x720(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm1, %ymm1
               	vpmaddubsw	0x740(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm2, %ymm2
               	vpmaddubsw	0x760(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm3, %ymm3
               	vpbroadcastd	0x3c(%rcx), %ymm5
               	vpmaddubsw	0x780(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm0, %ymm0
               	vpmaddubsw	0x7a0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm1, %ymm1
               	vpmaddubsw	0x7c0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm2, %ymm2
               	vpmaddubsw	0x7e0(%rdx), %ymm5, %ymm4
               	vpmaddwd	%ymm6, %ymm4, %ymm4
               	vpaddd	%ymm4, %ymm3, %ymm3
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
               	movq	0x40(%rsp), %rax
               	vmovdqu	%ymm0, (%rax)
               	vzeroupper
               	vmovaps	(%rsp), %xmm6
               	addq	$0x18, %rsp
