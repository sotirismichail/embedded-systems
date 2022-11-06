; author: S. Michail
; date: Thursday, October 12th, 2020
; 
; ss_disp.asm
;
; Description: A seven segment display using a timer and a lookup table
; Assuming a device frequency of 10MHz and common anode LEDs

.INCLUDE "M16DEF.INC"

.DSEG
.ORG	0x60
DATA_IN_RAM:	.BYTE	8

.CSEG
.MACRO INITSTACK						;Initialise the stack
	LDI	R20,HIGH(RAMEND)
	OUT SPH,R20
	LDI R20,LOW(RAMEND)
	OUT SPL,R20
.ENDMACRO

.MACRO DEMO_RAM
	LDI		R16,0b00000001				;Load some sample data into the RAM
	LDI		R17,0b00001000				;Load some sample data into the RAM
	LDI		R18,0b00000010				;Load some sample data into the RAM
	LDI		R19,0b00000001				;Load some sample data into the RAM
	LDI		R20,0b00000010				;Load some sample data into the RAM
	LDI		R21,0b00000000				;Load some sample data into the RAM
	LDI		R22,0b00000010				;Load some sample data into the RAM
	LDI		R23,0b00000001				;Load some sample data into the RAM
	LDI		XL,LOW(DATA_IN_RAM)			;The sample number is "18212021"
	LDI     XH,HIGH(DATA_IN_RAM)
	ST		X+,R16
	ST		X+,R17
	ST		X+,R18
	ST		X+,R19
	ST		X+,R20
	ST		X+,R21
	ST		X+,R22
	ST		X+,R23
	LDI		R16,0x00
	LDI		R17,0x00
	LDI		R18,0x00
	LDI		R19,0x00
	LDI		R20,0x00
	LDI		R21,0x00
	LDI		R22,0x00
	LDI		R23,0x00
.ENDMACRO

.ORG	0x20		;Lookup table of 7 segment codes
CODE_TABLE:
	.DB 0b11000000,0b11111001,0b10100100,0b10110000,0b10011001,0b10010010,0b10000010,0b11110000,0b10000000,0b10010000
	;		0			1		  2		      3		    4		   5		  6			  7			8			9

.ORG 0x0
	JMP		MAIN						;Jump to the main portion of the code after a reset

.ORG 0x12
	JMP		T0_OV_ISR					;Jump to the ovf handler

.ORG 0x100
MAIN:
	INITSTACK
	DEMO_RAM
	LDI		R16,0xFF
	LDI		R17,0xFF					;0b11111111, used to set all the pins as outputs
	OUT		DDRA,R16					;Outputs are CA LEDs, '0' ON, '1' OFF
	OUT		DDRC,R16
	OUT		PORTA,R16					
	OUT		PORTC,R17
	
	LDI		R20,(1<<TOIE0)				;Enable timer0 overflow interrupt
	OUT		TIMSK,R20	
	SEI									;Enable interrupts globally
	LDI		R20,0x58					;R20 = 214
	OUT		TCNT0,R20					
	LDI		R20,(1<<CS00)|(1<<CS02)		;Set the timer0 prescaler, f_timer0 = f_clk/1024
	OUT		TCCR0,R20					;=>f_timer0 ~= 10kHz => T_timer0 = 0,1ms
										;0,1 ms * (256 - 214) = 0,1 ms * 42 = 4,2 ms =>
										;240Hz display refresh rate
	LDI		ZH,HIGH(CODE_TABLE<<1)		
	LDI		R16,0xFF					;Initialize the 7 segments and ring counter with all off
	OUT		PORTA,R16				
	OUT		PORTC,R17
	LDI		R24,0b11111110				;Only one PIN on DDRC is active at a time
	OUT		PORTC,R24					;Send it to the ring counter, for the first digit to be on
	LDI		XH,HIGH(DATA_IN_RAM)		;Initialize RAM pointers, XH for the high address
	LD		R18,-X						;Load the first number from the RAM
	ANDI	R18,0b00001111				;Mask the upper 4 bits
	LDI		ZL,LOW(CODE_TABLE<<1)		;load from CODE_TABLE
	ADD		ZL,R18						;Add the BCD number to the table pointer
	LPM		R22,Z						;Get the data from the display table
	OUT		PORTA,R22					;Send it to the 7-segment LEDs		
DISP:
	CPI		R23,7						;Display the digit for some time
	BREQ	REINIT						;If we've gone through all 8 digits in the
	JMP		DISP						;RAM, we need to reinitialize the pointer
REINIT:
	LD		R23,X+						;Reset the X pointer, moving it 8 bytes forward
	LD		R23,X+
	LD		R23,X+
	LD		R23,X+
	LD		R23,X+
	LD		R23,X+
	LD		R23,X+
	LD		R23,X+
	LDI		R23,0
	JMP		DISP

.ORG 0x200
T0_OV_ISR:
	LDI		R16,0xFF					;Reset the display
	OUT		PORTA,R16
	OUT		PORTC,R16
	INC		R23							;Increase the digits we've gone through
	BST		R24,7
	ROL		R24							;Circular left shift with no carry
	BLD		R24,0
	OUT		PORTC,R24
	LD		R18,-X						;Load a new number from the RAM
	ANDI	R18,0b00001111				;Mask the upper 4 bits
	LDI		ZL,LOW(CODE_TABLE<<1)		;load from CODE_TABLE
	ADD		ZL,R18						;Add the BCD number to the table pointer
	LPM		R22,Z						;Get the data from the display table
	OUT		PORTA,R22					;Send it to the 7-segment LEDs		
	LDI		R20,0x58					;Reset the timer
	OUT		TCNT0,R20		
	RETI								;Return from interrupt