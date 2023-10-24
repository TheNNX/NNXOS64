%macro func 1
[GLOBAL %1]
    %1:
%endmacro

%define BOCHS_DEBUG xchg bx, bx

%macro pushstate 0
    push rdx
    push rcx
    push rbx
    push rax
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    push rbp
    push rdi
    push rsi
%endmacro

%macro popstate 0
    pop rsi
    pop rdi
    pop rbp
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rax
    pop rbx
    pop rcx
    pop rdx
%endmacro

%macro pushvol 0
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
%endmacro

%macro popvol 0
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
     pop rax
%endmacro

%macro pushnonvol 0
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rbp
    push rdi
    push rsi
    mov rbx, fs
    push rbx
    sub rsp, 8
    mov rbx, es
    push rbx
    mov rbx, ds
    push rbx
%endmacro

%macro popnonvol 0
    pop rbx
    mov ds, rbx
    pop rbx
    mov es, rbx
    ; Note: GS is skipped, as writing to GS on x64 clears the MSR IA32_GS_BASE,
    ; which makes it impossible to access the PCR
    add rsp, 8
    pop rbx
    mov fs, rbx
    pop rsi
    pop rdi
    pop rbp
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
%endmacro

%macro popvolnorax 0
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
%endmacro

%macro pushvolnorax 0
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
%endmacro