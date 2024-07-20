[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../CommonInclude/func.inc"
%include INC_FUNC

export NnxExitThread
func NnxExitThread
	mov r9, 5
	syscall
	ret

export NnxSyscall
func NnxSyscall
	mov r9, rcx
	syscall
	ret