[BITS 64]
[SECTION .TEXT]

[GLOBAL LoadIDT]
LoadIDT:
	lidt [rcx]
	ret

[EXTERN IntTestC]
[GLOBAL IntTestASM]
IntTestASM:
	call IntTestC
	iretq

[GLOBAL StoreIDT]
StoreIDT:
	sidt [rcx]
	ret

[GLOBAL EnableInterrupts]
EnableInterrupts:
	sti
	ret

[GLOBAL DisableInterrupts]
DisableInterrupts:
	cli
	ret

[GLOBAL ForceInterrupt]
ForceInterrupt:
	mov byte [intID], cl
	db 0xcd		//INT
intID: db 0x0	//immediate8
	ret