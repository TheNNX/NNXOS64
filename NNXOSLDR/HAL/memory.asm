[BITS 64]
[SECTION .TEXT]

[GLOBAL SetCR3]
SetCR3:
	mov cr3, rcx
	ret

[GLOBAL GetCR3]
GetCR3:
	mov rax, cr3
	ret

[GLOBAL SetCR0]
SetCR0:
	mov cr0, rcx
	ret

[GLOBAL GetCR0]
GetCR0:
	mov rax, cr0
	ret

[GLOBAL GetStack]
GetStack:
	mov rax, rsp
	ret

[GLOBAL SetStack]
	xchg rcx, rsp
	jmp qword [rcx]