[BITS 16]
[ORG 0x0000]

ApStartup:
.SpinWait:
	pause
	test WORD [ApSpinlock], 0
	jnz .SpinWait
.SpinAcquire:
	lock bts WORD [ApSpinlock], 0
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

BITS 64
LongModeEntry:
	mov ax, 0x10
	mov ds, ax
	mov ss, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	lidt [ApIdtr64]
	lgdt [ApGdtr64]

	; get the LAPIC ID into RBX and RCX
	mov rax, 0x0000000000000001
	cpuid
	shr rbx, 24
	and rbx, 0x00000000000000FF
	mov rcx, rbx

	; set the stack for this AP
	; jmp $
	mov QWORD rsi, [ApStackPointerArray]
	shl rbx, 3
	add rsi, rbx
	mov rsp, [rsi]

	; release the ApSpinlock before going to the C function
	lock btr WORD [ApSpinlock], 0

	mov QWORD rax, [ApProcessorInit]
	call rax

	; shouldn't return, if it did, do a cli+hlt loop
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

ApCurrentlyBeingInitialized:
								db 0x00 
ApCR3:
								dq 0x0000000000000000
ApStackPointerArray:
								dq 0x0000000000000000
ApProcessorInit:
								dq 0x0000000000000000
ApGdtr64:						
.Size:							dw 0x0000
.Base:							dq 0x0000000000000000
ApIdtr64:
.Size:							dw 0x0000
.Base:							dq 0x0000000000000000

times 4096 - ($ - ApStartup) db 0x00