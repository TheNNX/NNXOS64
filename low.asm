[BITS 64]
[SECTION .TEXT]
[GLOBAL GetCR3]
GetCR3:
	MOV RAX, CR3
	RET

[GLOBAL GetCR2]
GetCR2:
	MOV RAX, CR2
	RET

[GLOBAL GetCR4]
GetCR4:
	MOV RAX, CR2
	RET

[GLOBAL SetCR3]
SetCR3:
	MOV CR3, RCX
	RET