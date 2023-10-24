[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../CommonInclude/func.inc"
%include INC_FUNC

; new stack in rcx
; "return" address in rdx
[export SetupStack]
func SetupStack
    mov rsp, rcx
    jmp rdx

[export HalpGetCurrentAddress]
func HalpGetCurrentAddress
    mov rax, [rsp]
    ret