[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../../../CommonInclude/func.inc"
%include INC_FUNC

[extern PspScheduleThread]

func PspIdleThreadProcedure
    hlt
    jmp PspIdleThreadProcedure
