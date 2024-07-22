[BITS 64]
[SECTION .text]

%macro SYSCALL0_M 2
global Syscall%1
export Syscall%1

extern __imp_%1

Syscall%1:
    mov r9, %2
    syscall
    ret
%endmacro

%macro SYSCALL1_M 2
global Syscall%1
export Syscall%1

extern __imp_%1

Syscall%1:
    mov [rsp + 8], rcx
    mov r9, %2
    syscall
    ret
%endmacro

%macro SYSCALL2_M 2
global Syscall%1
export Syscall%1

extern __imp_%1

Syscall%1:
    mov [rsp + 8], rcx
    mov [rsp + 16], rdx
    mov r9, %2
    syscall
    ret
%endmacro

%macro SYSCALL3_M 2
global Syscall%1
export Syscall%1

extern __imp_%1

Syscall%1:
    mov [rsp + 8], rcx
    mov [rsp + 16], rdx
    mov [rsp + 24], r8
    mov r9, %2
    syscall
    ret
%endmacro

%macro SYSCALL4PLUS_M 2
global %1
export %1

%1:
    mov [rsp + 8], rcx
    mov [rsp + 16], rdx
    mov [rsp + 24], r8
    mov [rsp + 32], r9
    mov r9, %2
    syscall
    ret
%endmacro

%define SYSCALL0(x,y) SYSCALL0_M x,y
%define SYSCALL1(x,y) SYSCALL1_M x,y
%define SYSCALL2(x,y) SYSCALL2_M x,y
%define SYSCALL2(x,y) SYSCALL3_M x,y
%define SYSCALL(x,y,z) SYSCALL4PLUS_M x,y

%strcat INC_SYSCALL __FILE__,"/../../CommonInclude/syscall.inc"
%include INC_SYSCALL
