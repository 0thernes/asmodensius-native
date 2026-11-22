# morton_loop.s - the loop body of morton3_pdep_batch (kern.asm) in AT&T
# syntax for llvm-mca; copied from the llvm-objdump disassembly of kern.obj.
	movzwl	(%rcx,%r11,2), %eax
	movzwl	(%rdx,%r11,2), %r12d
	movzwl	(%r8,%r11,2), %r13d
	pdepl	%ebx, %eax, %eax
	pdepl	%esi, %r12d, %r12d
	pdepl	%edi, %r13d, %r13d
	orl	%r12d, %eax
	orl	%r13d, %eax
	movl	%eax, (%r9,%r11,4)
	incq	%r11
	cmpq	%r10, %r11
	jb	.L0
.L0:
