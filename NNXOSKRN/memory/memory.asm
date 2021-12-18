[BITS 64]
[SECTION .text]

[GLOBAL GetStack]
GetStack:
	MOV RAX, RSP
	RET

[GLOBAL SetStack]
SetStack:
	XCHG RCX, RSP
	JMP QWORD [RCX]

[GLOBAL PagingInvalidatePage]
PagingInvalidatePage:
	INVLPG [RCX]
	RET

[GLOBAL PagingTLBFlush]
PagingTLBFlush:
	MOV RAX, CR3
	MOV CR3, RAX
	RET
