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
