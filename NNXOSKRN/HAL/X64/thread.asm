[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../../../CommonInclude/func.inc"
%include INC_FUNC

[extern PspScheduleThread]

func HalpUpdateThreadKernelStack 
    push QWORD rdi
    mov QWORD rdi, [gs:0x08]
    add rcx, 152
    ; set RSP0
    mov QWORD [rdi+0x04], rcx
    ; set IST1 (used for ring 0 to ring 0 task switch)
    mov QWORD [rdi+0x24], rcx
    pop QWORD rdi
    ret

func HalpGetThreadKernelStack
    push QWORD rdi
    mov QWORD rdi, [gs:0x08]
    ; copy RSP0 into RAX
    mov rax, QWORD [rdi+0x04]

    ; UNCOMMENT THE BELOW FOR A DEBUG ASSERTION (IF RSP0 != IST1)
    ;xor rax, QWORD [rdi+0x24]
    ;jnz .DebugStackMismatch
    ;mov rax, QWORD [rdi+0x04]
    
    pop QWORD rdi
    ret
.DebugStckMismatch
    jmp $


[extern ApicSendEoi]
func HalpTaskSwitchHandler
    ; TODO: nested interrupts?
    ; interupts during sheduling and context switching are not desired
    ; other interupts should be nestable, though
    ; RFLAGS are already pushed onto the stack, no need to (re)store   
    cli
    
    cmp QWORD [rsp+8], 0x08
    je .noswap
    swapgs
.noswap:

    call PspStoreContextFrom64

    mov rcx, rsp
    call PspScheduleThread
    push QWORD rax
    call ApicSendEoi
    pop QWORD rcx
    jmp PspSwitchContextTo64

func PspStoreContextFrom64
    ; store RAX so we can use it for the return address
    ; +8 because we want to skip the return address on the stack
    mov QWORD [rsp-152+8], rax 
    ; pop the return address
    pop QWORD rax
    
    sub rsp, 152
    mov QWORD [rsp+8], rbx
    mov QWORD [rsp+16], rcx
    mov QWORD [rsp+24], rdx
    mov QWORD [rsp+32], rbp
    mov QWORD [rsp+40], rdi
    mov QWORD [rsp+48], rsi
    mov QWORD [rsp+56], r8
    mov QWORD [rsp+64], r9
    mov QWORD [rsp+72], r10
    mov QWORD [rsp+80], r11
    mov QWORD [rsp+88], r12
    mov QWORD [rsp+96], r13
    mov QWORD [rsp+104], r14
    mov QWORD [rsp+112], r15
    
    mov rbx, ds
    mov QWORD [rsp+120], rbx
    mov rbx, es
    mov QWORD [rsp+128], rbx
    mov rbx, fs
    mov QWORD [rsp+136], rbx
    ; mov rbx, gs
    ; mov QWORD [rsp+144], rbx
    
    jmp rax

func PspSwitchContextTo64
    mov rsp, rcx
    add rcx, 40
    call HalpUpdateThreadKernelStack
    mov QWORD rax, [rsp+120]
    mov ds, rax
    mov QWORD rax, [rsp+128]
    mov es, rax
    mov QWORD rax, [rsp+136]
    mov fs, rax
    ; mov QWORD rax, [rsp+144]
    ; mov gs, rax
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
    add rsp, 152
.return:
    cmp QWORD [rsp+8], 0x08
    je .noswap
    swapgs
.noswap:
    iretq

func PspTestAsmUser
    jmp $

func PspTestAsmUserEnd

func PspSchedulerNext
    push QWORD rax

    ; load new stack pointer into RAX
    call HalpGetThreadKernelStack

    ; exchange RAX and RSP:
    ;  the old stack pointer gets saved into RAX
    ;  the new stack pointer is loaded into RSP
    xchg rsp, rax

    ; simulate an interrupt

    ; order of pushing is SS, since we have RSP in RAX, we have to push two times to push RSP in its place 
    ; and later replace the first push manually
    push QWORD rax
    push QWORD rax
    mov QWORD rax, ss
    mov [rsp+8], rax

    ; store RFLAGS, CS, RIP
    pushf
    mov rax, cs
    push QWORD rax
    ; there's a link error if an absolute address is pushed here, since we're using RIP relative addressing (so the binary can be anywhere in the memory)
    ; to circuimvent that, we do a relative call, and thus have RIP on stack, and then we add the difference between RIP on stack and desired one
    call .next
    .next:
    mov QWORD rax, [rsp]
    add QWORD rax, .ReturnAddress - .next
    mov QWORD [rsp], rax

    cli
    
    cmp QWORD [rsp+8], 0x08
    je .noswap
    swapgs
.noswap:

    call PspStoreContextFrom64
    mov rcx, rsp
    call PspScheduleThread
    push QWORD rax
    pop QWORD rcx
    jmp PspSwitchContextTo64

.ReturnAddress:
    pop QWORD rax
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