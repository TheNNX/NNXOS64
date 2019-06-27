[BITS 64]
[SECTION .text]
[GLOBAL LoadGDT]
%define retfq o64 retf
LoadGDT:
	lgdt [rcx]
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	
	push qword 8
	push qword returnPoint
	retfq
returnPoint:
	ret

[GLOBAL StoreGDT]
StoreGDT:
	sgdt [rcx]
	ret

[GLOBAL LoadTSS]
LoadTSS:
	ltr cx
	ret