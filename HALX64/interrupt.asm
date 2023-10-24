[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../CommonInclude/func.inc"
%include INC_FUNC

export HalpDefInterruptHandler
func HalpDefInterruptHandler
    iretq

[export KiEnableInterrupts]
[export HalEnableInterrupts]
func KiEnableInterrupts
func HalEnableInterrupts
    sti
    ret

[export KiDisableInterrupts]
[export HalDisableInterrupts]
func KiDisableInterrupts
func HalDisableInterrupts
    cli
    ret

[export KeStop]
func KeStop
    cli
    hlt

[export HalpUpdateThreadKernelStack]
func HalpUpdateThreadKernelStack 
    push QWORD rdi
    mov QWORD rdi, [gs:0x08]
    ; set RSP0
    mov QWORD [rdi+0x04], rcx
    pop QWORD rdi
    ret

[export HalpGetThreadKernelStack]
func HalpGetThreadKernelStack
    push QWORD rdi
    mov QWORD rdi, [gs:0x08]
    ; copy RSP0 into RAX
    mov rax, QWORD [rdi+0x04]
    pop QWORD rdi
    ret