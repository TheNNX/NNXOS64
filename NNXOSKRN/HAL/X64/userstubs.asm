[BITS 64]
[SECTION .userc]
%strcat INC_FUNC __FILE__,"/../../../../CommonInclude/func.inc"
%include INC_FUNC

func KiUserRestoreUserContext
    mov rsp, rcx
    sub QWORD [rsp+176], 8
    mov QWORD rdi, [rsp+176]
    mov QWORD rax, [rsp+152]
    mov QWORD [rdi], rax
    mov QWORD rax, [rsp]
    mov QWORD rbx, [rsp+8]
    mov QWORD rcx, [rsp+16]
    mov QWORD rdx, [rsp+24]
    mov QWORD rbp, [rsp+32]
    mov QWORD rdi, [rsp+40]
    mov QWORD rsi, [rsp+48]
    mov QWORD r8,  [rsp+56]
    mov QWORD r9,  [rsp+64]
    mov QWORD r10, [rsp+72]
    mov QWORD r11, [rsp+80]
    mov QWORD r12, [rsp+88]
    mov QWORD r13, [rsp+96]
    mov QWORD r14, [rsp+104]
    mov QWORD r15, [rsp+112]
    mov QWORD rsp, [rsp+176]
    ret

func TestUserThread1
    xor rax, rax
.loop:
    mov rdx, rax
    mov r9, 1
    syscall
    mov rcx, 0x1FFFFFF
.decr:
    test rcx, rcx
    jz .enddecr
    dec rcx
    jmp .decr
.enddecr:

    jmp .loop

func TestUserThread2
    xor rax, rax
.loop:
    mov rdx, rax
    mov r9, 2
    syscall
    mov rcx, 0x3FFFFFF
.decr:
    test rcx, rcx
    jz .enddecr
    dec rcx
    jmp .decr
.enddecr:
    jmp .loop 

func TestFailUserThread
    ; causes a GPF
    hlt
    jmp $