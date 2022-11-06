; author: S. Michail
; date: Thursday, October 2nd, 2020
; 
; timer_delay.asm
;
; Description: A 1ms delay using nested loops
; Assuming a device frequency of 10MHz, 1 instruction cycle == 0.1 us

.INCLUDE "M16DEF.INC"

.ORG 0x00
		JMP		DELAY		;Jump to delay function in case of reset

.MACRO	INITSTACK
		LDI		R20,HIGH(RAMEND)
		OUT		SPH,R20
		LDI		R20,LOW(RAMEND)
		OUT		SPH,R20
.ENDMACRO

.ORG	0x100
DELAY:	INITSTACK			;Initializing stack
		LDI		R16,0x00
		LDI		R17,(1<<DDA0)
		OUT		PORTA,R16
		OUT		DDRA,R17	;PA0 is the output LED
		LDI		R16,8		;1 cycle
AGAIN:	LDI		R17,250		;1 cycle
HERE:	NOP					;1 cycle
		NOP					;1 cycle
		DEC		R17			;1 cycle
		BRNE	HERE		;2 cycles if it branches, 1 if it falls through	 (5 cycles x 250) - 1 cycle = 1249 cycles
		DEC		R16			;1 cycle
		BRNE	AGAIN		;2 cycles if it branches, 1 if it falls through  (4 cycles x 8) - 1 cycle = 31 cycles
																			;(8 x 1249) + 31 = 10023 cycles
																			;10023 x 0.1 us = 1002.3 us = 1.0023 ms 
		LDI		R17,(1<<PA0)
		OUT		PORTA,R17	;Turn LED on