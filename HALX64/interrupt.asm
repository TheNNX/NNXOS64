[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../CommonInclude/func.inc"
%include INC_FUNC

export HalpDefInterruptHandler
func HalpDefInterruptHandler
	iretq
