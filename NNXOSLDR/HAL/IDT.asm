[BITS 64]
[SECTION .text]
%include "func.inc"

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

%macro exception_error 1
[GLOBAL Exception%1]
Exception%1:
	pushstate
	mov rcx, %1
	mov rdx, [rsp+120]
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
	mov rdx, 0
	call ExceptionHandler
	popstate
	iretq
%endmacro

%macro irq 1
[GLOBAL IRQ%1]
IRQ%1:
	pushstate
	xchg bx, bx
	mov rcx, %1
	call IRQHandlerInternal
	popstate
	iretq
%endmacro

func LoadIDT
	lidt [rcx]
	ret

[EXTERN IntTestC]
func IntTestASM
	call IntTestC
	iretq

func StoreIDT
	sidt [rcx]
	ret

func EnableInterrupts
	sti
	ret

func DisableInterrupts
	cli
	ret

func ForceInterrupt
	mov byte [intID], cl
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
exception_error 14
exception 16
exception_error 17
exception 18
exception 19
exception 20
exception_error 30

[GLOBAL ExceptionReserved]
ExceptionReserved:
exception_error 0xffffffffffffffff

[EXTERN IRQHandler]
func IRQHandlerInternal
	mov al, 0x20
	out 0x20, al
	cmp rcx, 8
		jng .end
	out 0xA0, al
.end:
	sub rsp, 8			;M$ weird calling convention reserves 4 bytes (pushing 8, just in case)
	call IRQHandler
	add rsp, 8
	BOCHS_DEBUG
	ret

irq 0
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