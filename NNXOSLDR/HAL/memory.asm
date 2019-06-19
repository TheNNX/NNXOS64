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