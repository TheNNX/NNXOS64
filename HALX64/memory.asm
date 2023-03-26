[BITS 64]
[SECTION .text]

[global GetStack]
[export GetStack]
GetStack:
	MOV RAX, RSP
	RET

[global SetStack]
[export SetStack]
SetStack:
	XCHG RCX, RSP
	JMP QWORD [RCX]

[global PagingInvalidatePage]
[export PagingInvalidatePage]
PagingInvalidatePage:
	INVLPG [RCX]
	RET

[global PagingTLBFlush]
[export PagingTLBFlush]
PagingTLBFlush:
	MOV RAX, CR3
	MOV CR3, RAX
	RET
