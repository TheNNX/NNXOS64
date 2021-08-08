[BITS 64]
[SECTION .text]
%include "func.inc"

func GetRAX
	RET

%macro getreg 1
[GLOBAL GetR%1]
GetR%1:
	MOV QWORD RAX, R%1 
	RET
%endmacro

getreg BX
getreg CX
getreg DX
getreg SP
getreg BP
getreg DI
getreg SI

[GLOBAL SetCR3]
SetCR3:
	mov cr3, rcx
	ret

[GLOBAL GetCR3]
GetCR3:
	mov rax, cr3
	ret

[GLOBAL SetCR2]
SetCR2:
	mov cr2, rcx
	ret

[GLOBAL GetCR2]
GetCR2:
	mov rax, cr2
	ret

[GLOBAL SetCR0]
SetCR0:
	mov cr0, rcx
	ret

[GLOBAL GetCR0]
GetCR0:
	mov rax, cr0
	ret

[GLOBAL GetCR4]
GetCR4:
	mov rax, cr4
	ret

[GLOBAL SetCR4]
SetCR4:
	mov CR4, rcx
	ret
