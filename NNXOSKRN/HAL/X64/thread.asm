[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../../../CommonInclude/func.inc"
%include INC_FUNC

[extern PspScheduleThread]

func HalpUpdateThreadKernelStack 
    push QWORD rdi
    mov QWORD rdi, [gs:0x08]
    ; set RSP0
    add rcx, 192
    mov QWORD [rdi+0x04], rcx
    pop QWORD rdi
    ret

func HalpGetThreadKernelStack
    push QWORD rdi
    mov QWORD rdi, [gs:0x08]
    ; copy RSP0 into RAX
    mov rax, QWORD [rdi+0x04]
    sub rax, 192
    pop QWORD rdi
    ret

func PspIdleThreadProcedure
    hlt
    jmp PspIdleThreadProcedure

; rcx - number
; rdx - bit index
func TestBit
    
    ; test the rdx'th bit of rcx
    bt rcx, rdx

    ; subtraction with borrow, -1 if carry, 0 if not carry
    sbb rax, rax

    ; technicaly not needed, but let's normalize to 1
    and rax, 1

    ret

; rcx - number
; rdx - bit index
func ClearBit
    
    ; bit reset 
    btr rcx, rdx
    
    ; return rcx
    mov rax, rcx
    ret

func SetBit
    
    ; bit set
    bts rcx, rdx

    ; return rcx
    mov rax, rcx
    ret