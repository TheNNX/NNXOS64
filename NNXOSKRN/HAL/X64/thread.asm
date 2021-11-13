[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../../../CommonInclude/func.inc"
%include INC_FUNC

[extern PspScheduleThread]

func AsmLoop
	jmp $
	ret

; suspect
func HalpUpdateThreadKernelStack 
	push QWORD rdi
	mov QWORD rdi, [gs:0x08] 
	mov QWORD [rdi+0x04], rcx
	add QWORD [rdi+0x04], 152
	pop QWORD rdi
	ret

func HalpTaskSwitchHandler
	; disable interrupts, just in case
	pushf
	cli
	cmp QWORD [rsp+8], 0x08
	je .noswap
	swapgs
.noswap:
	popf

	call PspStoreContextFrom64

	mov rcx, rsp
	call PspScheduleThread
	mov rcx, rax
	jmp PspSwitchContextTo64

func PspStoreContextFrom64
	; store RAX so we can use it for the return address
	; +8 because we want to skip the return address on the stack
	mov QWORD [rsp-152+8], rax 
	; pop the return address
	pop QWORD rax

	sub rsp, 152
	mov QWORD [rsp+8], rbx
	mov QWORD [rsp+16], rcx
	mov QWORD [rsp+24], rdx
	mov QWORD [rsp+32], rbp
	mov QWORD [rsp+40], rdi
	mov QWORD [rsp+48], rsi
	mov QWORD [rsp+56], r8
	mov QWORD [rsp+64], r9
	mov QWORD [rsp+72], r10
	mov QWORD [rsp+80], r11
	mov QWORD [rsp+88], r12
	mov QWORD [rsp+96], r13
	mov QWORD [rsp+104], r14
	mov QWORD [rsp+112], r15
	
	mov rbx, ds
	mov QWORD [rsp+120], rbx
	mov rbx, es
	mov QWORD [rsp+128], rbx
	mov rbx, fs
	mov QWORD [rsp+136], rbx
	; mov rbx, gs
	; mov QWORD [rsp+144], rbx
	
	jmp rax

func PspSwitchContextTo64
	mov rsp, rcx
	add rcx, 40
	call HalpUpdateThreadKernelStack
	mov QWORD rax, [rsp+120]
	mov ds, rax
	mov QWORD rax, [rsp+128]
	mov es, rax
	mov QWORD rax, [rsp+136]
	mov fs, rax
	; mov QWORD rax, [rsp+144]
	; mov gs, rax
	mov QWORD rax, [rsp]
	mov QWORD rbx, [rsp+8]
	mov QWORD rcx, [rsp+16]
	mov QWORD rdx, [rsp+24]
	mov QWORD rbp, [rsp+32]
	mov QWORD rdi, [rsp+40]
	mov QWORD rsi, [rsp+48]
	mov QWORD r8,  [rsp+56]
	mov QWORD r9,  [rsp+64]
	mov QWORD r10, [rsp+72]
	mov QWORD r11, [rsp+80]
	mov QWORD r12, [rsp+88]
	mov QWORD r13, [rsp+96]
	mov QWORD r14, [rsp+104]
	mov QWORD r15, [rsp+112]
	add rsp, 152
.return:
	cmp QWORD [rsp+8], 0x08
	je .noswap
	swapgs
.noswap:
	iretq

func PspTestAsmUser
	jmp $
	mov rax, 0x1FFFFFF
.pointlessLoop
	cmp rax, 0
	je .end
	dec rax
	jmp .pointlessLoop
.end:
	int 0x20
	jmp PspTestAsmUser

func PspTestAsmUser2
	jmp $
	mov rax, 0xFFFFFF
.pointlessLoop
	cmp rax, 0
	je .end
	dec rax
	jmp .pointlessLoop
.end:
	int 0x20
	jmp PspTestAsmUser2

func PspTestAsmUserEnd
