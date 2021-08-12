[BITS 64]
[SECTION .text]

;for my convinience of not googling this over and over again:
;
;"That means RCX, RDX, R8, R9 for integer, struct or pointer arguments (in that order)"
;
;source, https://en.wikipedia.org/wiki/X86_calling_conventions
;
;
;

%strcat INC_FUNC __FILE__,"/../func.inc"
%include INC_FUNC

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

func DiskReadLong
	and rcx, 0xffff
	xchg rdx, rcx
	xchg rcx, r8
	push rdi
	mov rdi, r8
	cld
	rep insd
	pop rdi
	ret
