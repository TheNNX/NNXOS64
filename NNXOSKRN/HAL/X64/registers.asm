[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../../../CommonInclude/func.inc"
%include INC_FUNC

func KeStop
	cli
	hlt

; new stack in rcx
; "return" address in rdx
func SetupStack
	mov rsp, rcx
	jmp rdx

func HalpGetCurrentAddress
	mov rax, [rsp]
	ret