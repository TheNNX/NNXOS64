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

    pop QWORD rdi
    ret

func HalpGetThreadKernelStack
    push QWORD rdi
    mov QWORD rdi, [gs:0x08]
    ; copy RSP0 into RAX
    mov rax, QWORD [rdi+0x04]
    
    pop QWORD rdi
    ret
.DebugStckMismatch
    jmp $


[extern ApicSendEoi]
[extern KiSetIrql]
[extern KiGetCurrentThreadLocked]
[extern KiUnlockThread]
func HalpTaskSwitchHandler
    cli
    cmp QWORD [rsp+8], 0x08
    je .noswap
    swapgs
.noswap:
    sti

    ; save the thread register state to the kernel stack
    call PspStoreContextFrom64
    
    ; allocate the shadowspace
    sub rsp, 32
    
    ; set the IRQL to DISPATCH_LEVEL
    ; and store the old IRQL in a non-volatile register
    mov rcx, 2
    call KiSetIrql
    mov rbx, rax

    call KiGetCurrentThreadLocked
    mov r13, rax

    ; send an EOI to the APIC
    ; as such, the only thing that prohibits low-priority interrupts from firing
    ; is the IRQL
    call ApicSendEoi

    ; free the shadowspace
    add rsp, 32

    ; select the next thread
    mov rcx, rsp
    ; skip the shadowspace 
    call PspScheduleThread
    mov r12, rax

    ; allocate the shadowspace
    sub rsp, 32

    mov rcx, r13
    call KiUnlockThread

    mov rcx, rbx
    call KiSetIrql

    ; free the shadowspace
    add rsp, 32

    ; restore the selected thread state from the saved stack pointer
    mov rcx, r12
    jmp PspSwitchContextTo64

func PspStoreContextFrom64
    ; store RAX so we can use it for the return address

    ; 152 bytes is the size of the trap frame
    ; 8 bytes are already allocated by the caller for the return address
    ; Allocation size is adjusted accordingly, RAX is saved on the trap frame, 
    ; the return address is saved in RAX.
    
    ; Allocate the trap frame
    sub rsp, 152 - 8 
    
    ; Save RAX in its place on the trap frame
    mov QWORD [rsp], rax 
    
    ; pop the return address
    mov rax, QWORD [rsp+144]
    
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
    cli
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