[BITS 64]
[SECTION .text]
%define retfq o64 retf

[GLOBAL HalpLoadGdt]
HalpLoadGdt:
    lgdt [rcx]

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    push qword 8
    call .next
.next:
    mov qword rax, [rsp]
    add qword rax, .returnPoint - .next
    mov qword [rsp], rax
    retfq
.returnPoint:
    ret

[GLOBAL HalpLoadTss]
HalpLoadTss:
    ltr cx
    ret