[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../../../CommonInclude/func.inc"
%include INC_FUNC

func HalGetPcr
	mov QWORD rax, [gs:0x18]
	ret
