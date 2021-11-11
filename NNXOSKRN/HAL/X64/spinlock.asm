[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../../../CommonInclude/func.inc"
%include INC_FUNC

; lock pointer in RCX
func HalAcquireLockRaw
	push QWORD rsi
	mov rsi, rcx
.SpinWait:
	pause
	test QWORD [rsi], 0
	jnz .SpinWait
.SpinAcquire:
	lock bts WORD [rsi], 0
	jc .SpinWait
	pop QWORD rsi
	ret

; lock pointer in RCX
func HalReleaseLockRaw
	push QWORD rsi
	mov rsi, rcx
	lock btr QWORD [rsi], 0
	pop QWORD rsi
	ret