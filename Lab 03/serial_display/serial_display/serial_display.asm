; author: S. Michail
; date: Thursday, October 19th, 2020
; 
; ss_disp.asm
;
; Description: Implement an RS232 communication via the UART
; functions of the AVR micrcontroller. Receive commands from
; a PC, store ASCII digits to display as well as reply to the
; PC accordingly.

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

.ORG	0x20		;Lookup table of 7 segment codes
CODE_TABLE:
	.DB 0b11000000,0b11111001,0b10100100,0b10110000,0b10011001,0b10010010,0b10000010,0b11110000,0b10000000,0b10010000
	;		0			1		  2		      3		    4		   5		  6			  7			8			9

.ORG 0x0
	JMP		MAIN						;Jump to the main portion of the code after a reset

.ORG 0x12
	JMP		T0_OV_ISR					;Jump to the ovf handler

.ORG 0x16
	JMP		RXC_HANDLER					;RXC interrupt handler

.ORG 0x100
MAIN:
	INITSTACK							;Execute the stack initialization

	LDI		R16,0xFF					;0b11111111, used to set all the pins as outputs
	OUT		DDRA,R16					;Outputs are CA LEDs, '0' ON, '1' OFF
	OUT		DDRC,R16
	OUT		PORTA,R16					
	OUT		PORTC,R16

	LDI		R16,(1<<RXC)|(1<<TXC)					;Enable the UART interrupts
	OUT		UCSRA,R16
	LDI		R16,(1<<TXEN)|(1<<RXEN)|(1<<RXCIE)		;enable transmitter and receiver and the RXC interrupt
	OUT		UCSRB,R16
	LDI		R16,(1<<UCSZ1)|(1<<UCSZ0)|(1<<URSEL)	;no parity, 1 stop bit, write to UCSRC
	OUT		UCSRC,R16
	LDI		R16,0x05								;0x05 = 5, for 9600 baud calculated from:
	OUT		UBRRL,R16								;UBBRL value = (f_osc/(16 x 9600)) - 1 = 64, for f_osc = 1MHz 

	LDI		R20,(1<<TOIE0)				;Enable timer0 overflow interrupt
	OUT		TIMSK,R20	
	SEI									;Enable interrupts globally
	LDI		R20,0xFC					;R20 = 252
	OUT		TCNT0,R20					
	LDI		R20,(1<<CS00)|(1<<CS02)		;Set the timer0 prescaler, f_timer0 = f_clk/1024
	OUT		TCCR0,R20					;=>f_timer0 ~= 1kHz => T_timer0 = 1ms
										;1 ms * (256 - 252) = 1 ms * 4 = 4 ms =>
										;240Hz display refresh rate

	LDI		ZH,HIGH(CODE_TABLE<<1)	
	LDI		R23,0						;The amount of digits that will have been displayed
	LDI		R20,0						;The amount of digits that will be read from UART
	LDI		R24,0b11111110				;Only one PIN on DDRC is active at a time
	OUT		PORTC,R24					;Send it to the ring counter, for the first digit to be on
	LDI		XH,HIGH(DATA_IN_RAM)		;Initialize RAM pointers, XH for the high address
	LD		R18,-X						;Load the first number from the RAM
	CPI		R18,$0A
	BREQ	DISP						;If the digit is <LF>, then leave the 0xFF on PORTA
	ANDI	R18,0b00001111				;Mask the upper 4 bits
	LDI		ZL,LOW(CODE_TABLE<<1)		;load from CODE_TABLE
	ADD		ZL,R18						;Add the BCD number to the table pointer
	LPM		R22,Z						;Get the data from the display table	
	OUT		PORTA,R22					;Send it to the 7-segment LEDs
DISP:
	CP		R23,R20						;Display the digit for some time
	BREQ	REINIT_RAM_CALL				;If we've gone through all R20 digits in the
	JMP		DISP						;RAM, we need to reinitialize the pointer
REINIT_RAM_CALL:
	CALL	REINIT_RAM
	JMP		DISP

;**************************************************************
;* interrupt handler: RXC_HANDLER
;*
;* inputs: XH:XL - SRAM buffer address for received message
;*
;* outputs: none
;*
;* receives characters via UART and stores in data memory
;* until carriage return and line feed are received
;* Implements the three PC commands:
;*	-AT<CR><LF>   : Just reply
;*  -C<CR><LF>    : Clear screen and reply
;*  -N<X><CR><LF> : Store X to RAM and reply
;*
;* registers modified: r16, r17, R20, XL, XH
;**************************************************************
RXC_HANDLER:
	LDI		XH,HIGH(DATA_IN_RAM)		;Initialize RAM pointers, XH for the high address
	LDI		XL,LOW(DATA_IN_RAM)			;Initialize RAM pointers, XL for the low address
RCV:
	SBIS	UCSRA,RXC		;If there's no new data, return
	RJMP	RCV_END			;RXC is set when there's unread data, therefore we don't return 

	IN		R18,UDR			;Check if the received character is:

	CPI		R16,$41			;If we receive 'A'
	RJMP	RCV_AT			;wait for 'T'

	CPI		R16,$43			;'C', clear the screen
	LDI		R20,0
	CALL	SCRN_CLR
	RJMP	RCV_CR			;and wait for <CR><LF>

	CPI		R16,$4E			;'N', a number follows
	LDI		R20,0			;Reset the number of digits stored
	CALL	SCRN_CLR		;'N' also clears the screen
	RJMP	RCV_STORE		;go to store the number

RCV_LF:
	CPI		R16,$0A			;<LF>, end the receive and reply
	BREQ	RCV_END

RCV_CR:
	IN		R16,UDR
	CPI		R16,$0D			;If we receive <CR>, go to the state
	RJMP	RCV_LF			;waiting for the <LF>
	CALL	ERREPLY			;If it's not <CR>, reply with ERROR
	RETI

RCV_AT:
	IN		R16,UDR
	CPI		R16,$54			;If we receive 'T'
	RJMP	RCV_CR			;wait for the <LF>, else
	CALL	ERREPLY			;give an error reply, in case of an unrecognized command
	RETI					;and return

RCV_END:
	CALL	REPLY
	RETI

RCV_STORE:
	IN		R16,UDR			;Load the received character
	CPI		R16,$0D			;If it's <CR> then break to the <LF> sink state
	BREQ	RCV_LF						
	ST		X+,R16			;store the digit to RAM
	INC		R20				;Count the number of digits stored with R20
	CALL	SHIFT_RAM
	RJMP	RCV_STORE		;and get another from the receive buffer


;**************************************************************
;* subroutine: REINIT_RAM
;*
;* inputs: REGISTER - number of ASCII characters stored in RAM
;* Clears the RAM, and returns the X pointer to its 
;* starting position
;*
;* registers modified: r20,r22
;**************************************************************
REINIT_RAM:
	LDI		XH,HIGH(DATA_IN_RAM)		;Initialize RAM pointers, XH for the high address
	LDI		XL,LOW(DATA_IN_RAM)			;Initialize RAM pointers, XL for the low  address
REINIT_LOOP:
	LD		R22,X+						;Reset the X pointer, moving it 8 bytes forward
	CPI		R23,0
	BREQ	REINIT_RET
	DEC		R23
	RJMP	REINIT_LOOP
REINIT_RET:
	RET

;**************************************************************
;* subroutine: SHIFT_RAM
;*
;* inputs: REGISTER - number of ASCII characters stored in RAM
;* Shifts the digit received in the RAM, so it's stored at the
;* lowest address in the RAM, as the digits from the serial
;* port arrive.
;*
;* registers modified: r16,r17,r20,r22
;**************************************************************
SHIFT_RAM:
	LDI		XH,HIGH(DATA_IN_RAM)		;Initialize RAM pointers, XH for the high address
	LDI		XL,LOW(DATA_IN_RAM)			;Initialize RAM pointers, XL for the low  address
	LDI		R22,8						;Maximum digits we can store

	LD		R16,X+						;Load the first digit stored, and move the X pointer forward
	SUB		R22,R20						;Max digits - digits stored
	DEC		R22							;(max digits - digits stored) - 2
	DEC		R22
	CPI		R22,0
	BREQ	SHIFT_RET
SHIFT_LOOP:
	LD		R17,X+
	DEC		R22
	RJMP	SHIFT_LOOP
SHIFT_RET:
	ST		X+,R16
	LDI		XH,HIGH(DATA_IN_RAM)		;Initialize RAM pointers, XH for the high address
	LDI		XL,LOW(DATA_IN_RAM)			;Initialize RAM pointers, XL for the low  address
	RET

;**************************************************************
;* subroutine: REPLY
;*
;* inputs: none
;* sends the reply of OK<CR><LF> back to the PC
;*
;* registers modified: r17
;**************************************************************
REPLY:
	LDI		R17,'O'
	CALL	TRANSMT
	LDI		R17,'K'
	CALL	TRANSMT
	LDI		R17,$0D
	CALL	TRANSMT
	LDI		R17,$0A
	CALL	TRANSMT
	RET

;**************************************************************
;* subroutine: ERRREPLY
;*
;* inputs: none
;* sends the reply of ERROR<CR><LF> back to the PC
;*
;* registers modified: r17
;**************************************************************
ERREPLY:
	LDI		R17,'E'
	CALL	TRANSMT
	LDI		R17,'R'
	CALL	TRANSMT
	LDI		R17,'R'
	CALL	TRANSMT
	LDI		R17,'O'
	CALL	TRANSMT
	LDI		R17,'R'
	CALL	TRANSMT
	LDI		R17,$0D
	CALL	TRANSMT
	LDI		R17,$0A
	CALL	TRANSMT
	RET

;**************************************************************
;* subroutine: TRANSMT
;*
;* inputs: R17 - a character to transmit
;* transmits a single ASCII character via UART
;*
;* registers modified: r17
;**************************************************************
TRANSMT:
	SBIS	UCSRA,UDRE
	RJMP	TRANSMT
	OUT		UDR,R17
	RET

;**************************************************************
;* subroutine: SCRN_CLR
;*
;* inputs: none
;* Clears the 7 segment display screen memory
;*
;* registers modified: r16,r17
;**************************************************************
SCRN_CLR:
	LDI		R16,0xFF				;Initialize the 7 segments and ring counter with all off
	OUT		PORTA,R16
	OUT		PORTC,R16
	LDI     XH,HIGH(DATA_IN_RAM)
	LDI		XL,LOW(DATA_IN_RAM)
	LDI		R17,0
CLR_LOOP:
	ST		X+,R16
	INC		R17
	CPI		R17,8
	BREQ	CLR_RET
	RJMP	CLR_LOOP
CLR_RET:
	RET

;**************************************************************
;* interrupt handler: T0_OV_ISR
;*
;* inputs: none
;* handles the timer0 overflow interrupt for the
;* 7 segment display function
;*
;* registers modified: r16,r17,r18,r22,r23,r24,Z
;**************************************************************
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
	CPI		R18,$0A
	BREQ	DISP_RET					;If the digit is <LF>, then leave the 0xFF on PORTA
	ANDI	R18,0b00001111				;Mask the upper 4 bits
	LDI		ZL,LOW(CODE_TABLE<<1)		;load from CODE_TABLE
	ADD		ZL,R18						;Add the BCD number to the table pointer
	LPM		R22,Z						;Get the data from the display table
	OUT		PORTA,R22					;Send it to the 7-segment LEDs		
DISP_RET:
	LDI		R24,0xFC					;Reset the timer
	OUT		TCNT0,R24		
	RETI								;Return from interrupt
