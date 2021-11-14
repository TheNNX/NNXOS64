[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../../CommonInclude/func.inc"
%include INC_FUNC

func PicInitialize
	push rax

	mov al, 0x11
	out 0x20, al
	out 0xA0, al

	mov al, 0x20
	out 0x21, al
	mov al, 0x28
	out 0xA1, al

	mov al, 0x04
	out 0x21, al
	mov al, 0x02
	out 0xA1, al

	mov al, 0x01
	out 0x21, al
	out 0xA1, al

	mov al, 0x00
	out 0x21, al
	out 0xA1, al

	pop rax
	ret

func PicDisableForApic
	push rax

	mov al, 0xFF
	out 0x21, al
	out 0xA1, al

	pop rax
	ret