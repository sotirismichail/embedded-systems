/*
 * serial_display_asm.S
 *
 * Created: October 29th, 2020
 * Author : S. Michail
 * 
 *	Description
 */ 

 #include <avr/io.h>

 .global mask_ascii
	
	mask_ascii:
	ANDI	R24,0b00001111	;mask the upper 4 bits
	CLR		R25				;return value
	RET

.global timer0_handler

	timer0_handler:
	LDI		R18,0xFF					;Reset the display
	OUT		PORTA,R18
	IN		R19,PORTC
	BST		R19,0
	ROL		R19							;Circular left shift with no carry
	BLD		R19,7
	OUT		PORTC,R19
	RETI								;Return
