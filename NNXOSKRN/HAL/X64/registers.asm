[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../../../CommonInclude/func.inc"
%include INC_FUNC

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
getreg 8
getreg 9
getreg 10
getreg 11
getreg 12
getreg 13
getreg 14
getreg 15

[GLOBAL SetCR3]
SetCR3:
	mov cr3, rcx
	ret

[GLOBAL GetCR3]
GetCR3:
	mov rax, cr3
	ret

[GLOBAL SetCR2]
SetCR2:
	mov cr2, rcx
	ret

[GLOBAL GetCR2]
GetCR2:
	mov rax, cr2
	ret

[GLOBAL SetCR0]
SetCR0:
	mov cr0, rcx
	ret

[GLOBAL GetCR0]
GetCR0:
	mov rax, cr0
	ret

[GLOBAL GetCR4]
GetCR4:
	mov rax, cr4
	ret

[GLOBAL SetCR4]
SetCR4:
	mov cr4, rcx
	ret

[GLOBAL SetCR8]
SetCR8:
	mov cr8, rcx
	ret

[GLOBAL GetCR8]
GetCR8:
	mov rax, cr8
	ret

func KeStop
	cli
	hlt

func HalX64WriteMsr
	mov rax, rdx
	and rax, 0xFFFFFFFF
	shr rdx, 32
	wrmsr
	ret

func HalX64SwapGs
	swapgs
	mov rax, gs
	ret

; new stack in rcx
; "return" address in rdx
func SetupStack
	mov rsp, rcx
	jmp rdx