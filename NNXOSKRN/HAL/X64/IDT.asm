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

%macro pushnonvol 0
	push rbx
	push r12
	push r13
	push r14
	push r15
	push rbp
	push rdi
	push rsi
	mov rbx, fs
	push rbx
	sub rsp, 8
	mov rbx, es
	push rbx
	mov rbx, ds
	push rbx
%endmacro

%macro popnonvol 0
	pop rbx
	mov ds, rbx
	pop rbx
	mov es, rbx
	; Note: GS is skipped, as writing to GS on x64 clears the MSR IA32_GS_BASE,
	; which makes it impossible to access the PCR
	add rsp, 8
	pop rbx
	mov fs, rbx
	pop rsi
	pop rdi
	pop rbp
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbx
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
	add rsp, 8
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
	add rsp, 8
	pushstate
	mov rcx, %1
	xor rdx, rdx
	xor r8, r8
	mov r9, [rsp+120]
	call ExceptionHandler
	popstate
	iretq
%endmacro

[extern HalFullCtxInterruptHandlerEntry]
[extern HalGenericInterruptHandlerEntry]
[GLOBAL IrqHandler]
IrqHandler:
    cli
	; RAX is pushed on top of the usual stuff pushed
	; by the CPU by the handler stub.
	; Adjust the RSP offset accordingly.
    cmp QWORD [rsp+16], 0x08
    je .noswap
    swapgs
.noswap:
	sti

	; RAX is already pushed.
	pushvolnorax
	; Move the pointer to the interrupt object
	; to the first argument of the function call.
	mov rcx, rax
	push rcx
	sub rsp, 32
	call HalGenericInterruptHandlerEntry
	add rsp, 32
	pop rcx
	test al, al
	jnz .fullCtxRequired
	jmp HalpApplyTaskState.EnterThread
.fullCtxRequired:

	pushnonvol

	; RCX is restored after calling HalGenericInterruptHandlerEntry.
	mov rdx, rsp
	sub rsp, 32
	call HalFullCtxInterruptHandlerEntry
	test rax, rax
	jnz .changeStack
.noStackChange:
	add rsp, 32
	jmp .postStackAdjust
.changeStack:
	mov rsp, rax
.postStackAdjust:

	popnonvol

	jmp HalpApplyTaskState.EnterThread

func HalpApplyTaskState
	mov rsp, rcx

	popnonvol

.EnterThread:
	popvol
	cli
	push rcx
	mov rcx, rsp
	call HalpUpdateThreadKernelStack
	pop rcx
    cmp QWORD [rsp+8], 0x08
    je .noswap2
    swapgs
.noswap2:
	iretq

int 3
int 3
int 3

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
	cli
	add rsp, 8
    ;cmp QWORD [rsp+8], 0x08
    ;je .noswap
    ;swapgs
;.noswap:

	pushvol
	mov rcx, 14
	mov rdx, [rsp+56]
	xor r8, r8
	mov r9, [rsp+64]
	sub rsp, 32
	call PageFaultHandler
	add rsp, 32
	popvol

    cmp QWORD [rsp+8], 0x08
    je .noswap2
    swapgs
.noswap2:
	iretq

[GLOBAL ExceptionReserved]
ExceptionReserved:
exception_error 0xffffffffffffffff

[extern HalpUpdateThreadKernelStack]
[extern HalpSystemCallHandler]
func HalpSystemCall
	; interrupts should be disabled here at the start
	cli

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

	cli
	push rcx
	mov rcx, rsp
	; unreserve syscall handler data
	add rcx, 16
	call HalpUpdateThreadKernelStack
	pop rcx

	; get the user stack
	pop QWORD [gs:0x28]
	mov rsp, [gs:0x28]

	; restore the usermode GS
	swapgs

	; this restores RFLAGS, so it reenables interrupts too
	o64 sysret

; Handler address in RCX
func HalpMockupInterruptHandler
	; store the return address in a volatile register
	pop r8
	mov rax, ss
	mov r9, rsp
	; push SS
	push rax
	; push RSP
	push r9
	; push RFLAGS
	pushf
	; push CS
	mov rax, cs
	push rax
	; push RIP
	push r8
	jmp rcx