[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../../../CommonInclude/func.inc"
%include INC_FUNC

[extern PspScheduleThread]

func PspIdleThreadProcedure
    hlt
    jmp PspIdleThreadProcedure

; RCX - Arg0
; RDX - Arg1
; R8  - Service
; R9 - NumberOfStackArgs
; RSP[8 + 32 + 32] - AdjustedUserStack
func KiInvokeServiceHelper
    
    sub rsp, 32
    


    call [rax]
    add rsp, 32
    ret