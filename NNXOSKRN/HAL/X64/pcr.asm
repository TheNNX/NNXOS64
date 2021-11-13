[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../../../CommonInclude/func.inc"
%include INC_FUNC

func HalGetPcr
	mov QWORD rax, [gs:0x18]
	ret

extern HalX64SwapGs
func HalSwapGsInIfNecessary
	pushf
	cli
	push QWORD rax
	mov rax, ds
	cmp rax, 0x10
		jne .notNeeded
	call HalX64SwapGs
.notNeeded:
.done:
	pop QWORD rax
	popf
	ret