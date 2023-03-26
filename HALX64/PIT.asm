[BITS 64]
[SECTION .text]
%strcat INC_FUNC __FILE__,"/../../CommonInclude/func.inc"
%include INC_FUNC

; base frequency - 1193182 Hz

[export PitUniprocessorInitialize]
func PitUniprocessorInitialize
	push rax
	; ensure interrupts are off
	cli

	; channel 0, acc mode high and low byte, operation mode 0, bcd 0
	; channel 00  
	; acc mode  11
	; op mode     000
	; bcd            0
	mov al, 0b00110000
	out 0x43, al

	pop rax
	ret

[export PitUniprocessorPollSleepMs]
func PitUniprocessorPollSleepMs
	push rdx
	push rcx

	xor rdx, rdx
	movzx rax, cx
	mov rcx, 1193182
	imul rcx

	mov rcx, 1000
	idiv rcx

	mov rcx, rax
	and rcx, 0xFFFF
	shr rax, 16

	push rcx
.fullWaits:
	push rax
	mov rcx, 0xFFFF
	call PitUniprocessorPollSleepTicks
	pop rax

	cmp rax, 0
		jz .partialWait
	dec rax
	jmp .fullWaits
.partialWait:
	pop rcx
	call PitUniprocessorPollSleepTicks
	pop rcx
	pop rdx
	ret

[export PitUniprocessorPollSleepTicks]
func PitUniprocessorPollSleepTicks
	mov ax, cx
	out 0x40, al
	xchg al, ah
	out 0x40, al

.waiting:
	mov al, 0xE2
	out 0x43, al

	in al, 0x40
	and al, 0x80

	or al, al
	jnz .endwaiting
	jmp .waiting
.endwaiting:
	ret

[export PitUniprocessorSetupCalibrationSleep]
func PitUniprocessorSetupCalibrationSleep	

	; roughly a 1/20th of a second in ticks
	mov ax, 59659
	out 0x40, al
	xchg al, ah
	out 0x40, al

	ret

[export PitUniprocessorStartCalibrationSleep]
func PitUniprocessorStartCalibrationSleep
	jmp PitUniprocessorPollSleepTicks.waiting