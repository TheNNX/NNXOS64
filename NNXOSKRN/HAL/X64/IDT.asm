[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../../../CommonInclude/func.inc"
%include INC_FUNC

%macro pushstate 0
	push rdx
	push rcx
	push rbx
	push rax
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15
	push rbp
	push rdi
	push rsi
%endmacro

%macro popstate 0
	pop rsi
	pop rdi
	pop rbp
	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rax
	pop rbx
	pop rcx
	pop rdx
%endmacro

%macro pushvol 0
	push rax
	push rcx
	push rdx
	push r8
	push r9
	push r10
	push r11
%endmacro

%macro popvol 0
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
 	pop rax
%endmacro

%macro popvolnorax 0
	pop r11
	pop r10
	pop r9
	pop r8
	pop rdx
	pop rcx
%endmacro

%macro pushvolnorax 0
	push rcx
	push rdx
	push r8
	push r9
	push r10
	push r11
%endmacro

%macro exception_error 1
[GLOBAL Exception%1]
Exception%1:
	pushstate
	mov rcx, %1
	mov rdx, [rsp+120]
	xor r8, r8
	mov r9, [rsp+128]
	call ExceptionHandler
	popstate
	add rsp, 8
	iretq
%endmacro

%macro exception 1
[GLOBAL Exception%1]
Exception%1:
	pushstate
	mov rcx, %1
	xor rdx, rdx
	xor r8, r8
	mov r9, [rsp+120]
	call ExceptionHandler
	popstate
	iretq
%endmacro

[extern ApicSendEoi]
%macro irq 1
[GLOBAL IRQ%1]
IRQ%1:
    cli
    cmp QWORD [rsp+8], 0x08
    je .noswap
    swapgs
.noswap:
    sti

	pushstate

	mov rcx, %1

	sub rsp, 32
	call IrqHandlerInternal
	call ApicSendEoi
	add rsp, 32

	popstate

	cli
    cmp QWORD [rsp+8], 0x08
    je .noswap2
    swapgs
.noswap2:
    sti
	iretq
%endmacro

func HalpLoadIdt
	lidt [rcx]
	ret

func HalpDefInterruptHandler
	iretq

func HalpStoreIdt
	sidt [rcx]
	ret

func KiEnableInterrupts
func EnableInterrupts
	sti
	ret

func KiDisableInterrupts
func DisableInterrupts
	cli
	ret

func ForceInterrupt
	push qword rdi
	mov rdi, intID
	mov byte [rdi], cl
	pop qword rdi
	db 0xcd		;INT
intID: db 0x0	;immediate8
	ret

[EXTERN ExceptionHandler]

; exception handling

exception 0
exception 1
exception 2
exception 3
exception 4
exception 5
exception 6
exception 7
exception_error 8
exception_error 10
exception_error 11
exception_error 12
exception_error 13
exception 16
exception_error 17
exception 18
exception 19
exception 20
exception_error 30

[extern PageFaultHandler]
func Exception14
	jmp $
	cli
    cmp QWORD [rsp+8], 0x08
    je .noswap
    swapgs
.noswap:
    sti

	pushstate
	sub rsp, 32
	mov rcx, 14
	mov rdx, [rsp+56]
	xor r8, r8
	mov r9, [rsp+64]
	call PageFaultHandler
	add rsp, 32
	popstate

	cli
    cmp QWORD [rsp+8], 0x08
    je .noswap2
    swapgs
.noswap2:
	sti

	add rsp, 8
	iretq

[GLOBAL ExceptionReserved]
ExceptionReserved:
exception_error 0xffffffffffffffff

[EXTERN IrqHandler]
func IrqHandlerInternal
	sub rsp, 32
	call IrqHandler
	add rsp, 32
	ret

irq 1
irq 2
irq 3
irq 4
irq 5
irq 6
irq 7
irq 8
irq 9
irq 10
irq 11
irq 12
irq 13
irq 14

[extern HalpSystemCallHandler]
func HalpSystemCall
	; interrupts should be disabled here at the start
	
	; all syscalls are from usermode, swap the KernelGSBase with the PCR into GS
	swapgs

	; store the user stack to a PCR temp variable
	mov [gs:0x28], rsp

	; get the TSS pointer to RSP
	mov rsp, [gs:0x08]

	; get the RSP0 into RSP
	mov rsp, QWORD [rsp+0x04]

	; store the user stack on the kernel stack (we can't rely on the temp variables,
	; once interrupts are reenabled)
	push QWORD [gs:0x28]

	; reenable interrupts
	sti

	; store the register state
	pushvolnorax

	; allocate the shadow space
	sub rsp, 32

	mov rcx, r9

	; call the syscall handler 
	mov rax, HalpSystemCallHandler
	call [rax]

	; deallocate the shadowspace
	add rsp, 32

	; restore the register state
	popvolnorax

	; disable interrupts to prevent race conditions from occuring
	cli

	; get the user stack
	pop QWORD [gs:0x28]
	mov rsp, [gs:0x28]

	; restore the usermode GS
	swapgs

	; this restores RFLAGS, so it reenables interrupts too
	o64 sysret