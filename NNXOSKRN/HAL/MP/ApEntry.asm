[BITS 16]
[ORG 0x0000]

ApStartup:
.SpinWait:
	pause
	test word [ApSpinlock], 0
	jnz .SpinWait
.SpinAcquire:
	lock bts word [ApSpinlock], 0
	jc .SpinWait
.EnterLongMode:

	lidt [TempIDT.Pointer]

	mov eax, 0b10100000
	mov cr4, eax

	mov eax, [ApCR3]
	mov cr3, eax

	mov ecx, 0xC0000080
	rdmsr

	or eax, 0x00000100
	wrmsr

	jmp $
	mov eax, cr0
	or eax, 0x80000001
	mov cr0, eax

	lgdt [TempGDT.Pointer]

	jmp 0x0008:LongModeEntry

TempIDT:
	align 4
.Pointer:
	dw 0x0000
	dd 0x00000000

TempGDT:
.NullDesc:
	dq 0x0000000000000000
.CodeSeg:
	dw 0xFFFF
	dw 0x0000
	db 0x00
	db 0x9A
	db 0x2F
	db 0x00
.DataSeg:
	dw 0xFFFF
	dw 0x0000
	db 0x00
	db 0x92
	db 0x0F
	db 0x00
.GdtEnd:
	align 4
.Pointer:
	dw (.GdtEnd - TempGDT)
	dd TempGDT

LongModeEntry:
	jmp Error

Error:
	cli
	hlt
	jmp Error

Alignment:
times 0x800 - (Alignment - ApStartup) db 0x00
Data:

ApSpinlock: DW 0x0000
.End:
align 64

ApCurrentlyBeingInitialized: db 0x00 
ApGdtrPointer:		dw 0x0000
ApIdtrPointer:		dw 0x0000
ApCR3:				dq 0x0000000000000000
ApStackPointer16:	dw 0x0000
ApStackPointer64:	dq 0x0000000000000000

times 4096 - ($ - ApStartup) db 0x00