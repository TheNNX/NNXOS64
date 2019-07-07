[BITS 64]
[SECTION .text]

%include "func.inc"

func inb
	push rdx
	mov dx, cx
	in al, dx
	pop rdx
	ret

func inw
	push rdx
	mov dx, cx
	in ax, dx
	pop rdx
	ret

func ind
	push rdx
	mov dx, cx
	in eax, dx
	pop rdx
	ret

func outb
	push rdx
	push rcx
	mov al, dl
	mov dx, cx
	out dx, al
	pop rcx
	pop rdx
	ret

func outw
	push rdx
	push rcx
	mov ax, dx
	mov dx, cx
	out dx, ax
	pop rcx
	pop rdx
	ret

func outd
	push rdx
	push rcx
	mov eax, edx
	mov dx, cx
	out dx, eax
	pop rcx
	pop rdx
	ret
