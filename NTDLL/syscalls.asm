[BITS 64]
[SECTION .text]

%macro SYSCALL1_M 1
global Syscall%1
export Syscall%1
%endmacro

%macro SYSCALL2_M 1
global Syscall%1
export Syscall%1
%endmacro

%macro SYSCALL3_M 1
global Syscall%1
export Syscall%1
%endmacro

%macro SYSCALL4PLUS_M 1
global Syscall%1
export Syscall%1

extern __imp_%1

Syscall%1:
    mov [rsp + 8], rcx
    mov [rsp + 16], rdx
    mov [rsp + 24], r8
    mov [rsp + 32], r9
    mov r9, [rel __imp_%1]
    syscall
    ret
%endmacro

%define SYSCALL4PLUS(x) SYSCALL4PLUS_M x

%strcat INC_SYSCALL __FILE__,"/../../CommonInclude/syscall.inc"
%include INC_SYSCALL
