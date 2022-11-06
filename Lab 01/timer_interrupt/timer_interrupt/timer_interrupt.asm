; author: S. Michail
; date: Thursday, October 1st, 2020
; 
; timer_interrupt.asm
;
; Description: A 1ms delay using timer0 overflow interrupt
; Assuming a device frequency of 10MHz


.INCLUDE "M16DEF.INC"

.MACRO	INITSTACK
		LDI		R20,HIGH(RAMEND)
		OUT		SPH,R20
		LDI		R20,LOW(RAMEND)				;Initialize the stack
		OUT		SPL,R20				
.ENDMACRO

.ORG	0x0
		JMP		MAIN

.ORG	0x12
		JMP		T0_OV_ISR					;Set the address for the timer0 interrupt handler

.ORG	0x100
MAIN:	INITSTACK
		LDI		R16,0x00
		LDI		R17,(1<<DDA0)
		OUT		PORTA,R16		
		OUT		DDRA,R17					;Set PA0 as output LED pin
		LDI		R20,(1<<TOIE0)				;Enable timer0 overflow interrupt
		OUT		TIMSK,R20
		SEI									;Set I bit, enabling global interrupts
		LDI		R20,0xF6					;R20 = 246, timer value for 1ms
		OUT		TCNT0,R20
		LDI		R20,(1<<CS00)|(1<<CS02)		;Setting the timer0 bits for
		OUT		TCCR0,R20					;normal interal clock, prescaler set at: 
											;f_timer0 = f_clk/1024 => 
											;f_timer0 = ~10kHz => T_timer0 = 0,1ms
											;0,1ms * (256 - 246) = 0,1ms * 10 = 1ms, the target timer
										
HERE:	NOP									;Kill time waiting for the interrupt
		JMP		HERE		

.ORG	0x200
T0_OV_ISR:									;Timer0 OVF interrupt handler
		LDI		R16,(1<<PA0)
		OUT		PORTA,R16					;Turn on the LED to show the timer has finished
		LDI		R20,0xF6
		OUT		TCNT0,R20					;Reload the timer value for the next round
		RETI								;Return from interrupt