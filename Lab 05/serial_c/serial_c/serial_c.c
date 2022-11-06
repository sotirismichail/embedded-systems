/*
 *	serial_c.c
 *
 *	Created: 11/10/2020 2:08:31 PM
 *	Author : S. Michail
 *
 *	Programming an AVR microcontroller to parse simple
 *  commands, such as storing a number in the RAM and 
 *  displaying it on a seven-segment LED display.
 *
 *	Using the STK500 on-board software clock generator,
 *	set at 3,68MHz (its max value), for optimal USART
 *	communication with a PC.
 */ 

#include "serial_c.h"

/*
 * Initialization functions
 * - Output
 * - UART interface, setting two-way communication in the correct baud rate
 * - timer0, for the display refresh rate
 */
void 
init_output(void)
{
	DDRA = 0xFF;			//Set PORTA as output
	DDRC = 0xFF;			//Set PORTC as output
	PORTA = 0xFF;			//Default: all LEDs set high for off
	PORTC = 0xFF;			//as we have CA LEDs
}

void 
init_timer0(void)
{
	TIMSK = (1<<TOIE0);				//Enabling timer0 OVF interrupt
	TCCR0 = (1<<CS00)|(1<<CS02);	//f_clk/1024 prescaler setting => T_timer0 = 0,27 ms
	TCNT0 = 254;					//256 - 254 = 2 => (2 * 0,27 ms) x 8 => Display refreshes every 4,32 ms
}

/*	Using a 3,68 MHz clock, we can achieve 0% baud rate error.
 *	Due to the way the UBRRL value is calculated, any clock value
 *	that is a multiple of 1.8432 MHz produces a baud error rate
 *	very close to 0%. For a 3,68 MHz clock, the following 
 *	UBRRL values can be used for the desired baud rate:
 *
 *	Baud rate		UBRRL
 *	1200			191
 *	2400			95
 *	4800			47
 *	9600			23
 *	14400			15
 *	19200			11
 *	28800			7
 *	38400			5
 *	57600			3
 *	76800			2
 *	115200			1
 */
void
init_uart(void)
{
	UCSRB = (1<<TXEN)|(1<<RXEN)|(1<<RXCIE);		//Enable transmit, receive and receive complete interrupt
	UCSRC = (1<<UCSZ1)|(1<<UCSZ0)|(1<<URSEL);	//1 stop bit, no parity, and write to UBRRL
	UBRRL = 23;									//Value for 19200 baud
}	

/*
 *	function: uart_send
 *	description: Waits until the USART transmission buffer is empty
 *				 to transmit a character through the RS232 port.
 *
 *	arg0: A character to be transmitted
 *	
 *	returns: void
 */
void 
uart_send(char ch)
{
	while (! (UCSRA & (1 << UDRE)));	//Wait until UDR is empty
	UDR = ch;							//transmit the character
}

/*
 *	function: uart_puts
 *	description: Iterates through a string and sends each character individually
 *				 to be transmitted through uart_send.
 *
 *	arg0: A string pointer
 *	
 *	returns: void
 */
void 
uart_puts (const char *str)
{
	while (*str)
	{
		uart_send(*str++);	//Send each character individually
	}
}

/*
 *	function: clr_buffer
 *	description: Clears the display RAM buffer that stores the data from the
 *				 "N<X>\r\n" command. 'E' denotes an empty space.
 *
 *	arg0: void
 *	
 *	returns: void
 */
void 
clr_buffer()
{
	unsigned char i;
	
	for (i = 0; i < 8; i++) {
		ram_data[i] = 'E';			//10 == empty
	}
}

/*
 *	function: ret_buffer
 *	description: Transmits the numbers that are stored in the RAM with the 
 *				 "N<X>\r\n" command to the connected PC.
 *
 *	arg0: void
 *	
 *	returns: void
 */
void 
ret_buffer()
{
	uart_puts(ram_data);				//Print the memory buffer
	uart_puts(RETURN_NEWLINE);
}
 
 /*
 *	function: set_buffer
 *	description: Used to store the <X> numbers from the "N<X>\r\n" command.
 *				 Discards the data and returns an error if there are any
 *				 characters other than numerals and more than the specified
 *				 number of them (in this case, 8).
 *				 
 *				 ram_buffer is a RAM location at the address 0x006B
 *
 *	arg0: void
 *	
 *	returns: void
 */
unsigned char
set_buffer() 
{
	unsigned char ch, i = 0;

receive_data:
	while (! (UCSRA & (1 << RXC)));		//wait for new data
	ch = UDR;							//If the character is <LF>
	
	if (i < 8) {
		if (ch != CHAR_RETURN) {
			if (ch >= '0' && ch <= '9') {
				strcpy(ram_data[i], ch);
				i++;
				goto receive_data;
			} else {
				return FALSE;
			}
		}
	}
	
	while (i < 8) {
		ram_data[i] = 'E';	//Fill the empty display buffer spaces with
		i++;				//10 which signifies empty if we have received
	}						//less than 8 numerals, to prevent trailing zeroes
	
	return TRUE;
}

/*
 *	function: string_terminating 
 *	description: Checks if the commands received follow the specified format of
 *				 "COMMAND\r\n", with the appropriate terminating characters.
 *				 If they don't, the program returns an error.
 *
 *	arg0: void
 *	
 *	returns: TRUE or FALSE
 */
unsigned char 
string_terminating()
{
	unsigned char ch;
	
	while (! (UCSRA & (1 << RXC)));		//wait for new data
	ch = UDR;
	if (ch != CHAR_RETURN)
		return FALSE;
	while (! (UCSRA & (1 << RXC)));		//wait for new data
	ch = UDR;
	if (ch != CHAR_NEWLINE)
		return FALSE;
	
	return TRUE;
}

/*
 *	function: (ISR) USART RX RECEIVE Interrupt Handler
 *	description: Triggered every time the RS232 receives new data,
 *				 it calls the appropriate command processing function
 *				 depending on the command received. If a command is
 *				 not recognised, an ERROR is returned to the PC.
 */
ISR(USART_RXC_vect)
{
	unsigned char ch;
	
	ch = UDR;
	
	switch (ch) {
		case 'A':
			while (! (UCSRA & (1 << RXC)));		//wait for new data
			ch = UDR;
			if (ch == 'T') {
				if (string_terminating()) {
					uart_puts(REPLY_OK);
					reti();
				} else {
					uart_puts(REPLY_FAILURE);
					reti();
				}
			} else {
				uart_puts(REPLY_FAILURE);
				reti();
			}
			reti();
			break;
		case 'C':
			if (string_terminating()) {
				clr_buffer();
				uart_puts(REPLY_OK);
				reti();
			} else {
				uart_puts(REPLY_FAILURE);
				reti();
			}
			reti();
			break;
		case 'D':
			if (string_terminating()) {
				ret_buffer();
				reti();
			} else {
				uart_puts(REPLY_FAILURE);
				reti();
			}
			reti();
			break;
		case 'N':
			if (set_buffer()) {
				uart_puts(REPLY_OK);
				reti();
			} else {
				uart_puts(REPLY_FAILURE);
				reti();
			}
			reti();
			break;
		default:
			uart_puts(REPLY_FAILURE);
			reti();
	}
}

/*
 *	function: (ISR) TIMER0 Overflow Interrupt Handler
 *	description: Triggered every time timer0 overflows, it used to 
 *				 point to the next single digit of the 8 digit seven
 *				 segment display, and activate it through PORT B.
 *				 In this way, we can achieve a simultaneous display
 *				 of all 8 digits on the display for the human eye.
 *
 *				 Assuming the seven segment displays are of CA type
 *				 (active low)
 */
ISR(TIMER0_OVF_vect)
{
	active_segment++;
	if (active_segment > 7) {
		active_segment = 0;
	}
	PORTA = 0xFF;
	PORTC = 0xFF;
	PORTC &= ~(1 << active_segment);		//Set all but the active segment high
}

/*
 * main
 *
 * description: Initializes all outputs and interrupt handlers,
 *				and drives the physical seven segment display,
 *				getting the seven segment driving code from
 *				a lookup table, converting the stored ASCII
 *				character to an integer, to point to the 
 *				appropriate code.
 */				
int 
main(void)
{
	init_output();					//Initialize the physical output (PORT A & PORT B)
	clr_buffer();					//Initialize the RAM data space 				
	init_uart();					//Initialize the UART RS232 communication
	init_timer0();					//Initialize timer0 
	active_segment = 0;				//Initialize the active single digit display
	sei();							//Enable interrupts globally

    for (;;) 
    {
		if (ram_data[active_segment] == 'E') {	//'E' denotes an empty space in the RAM. To prevent showing
			PORTA = 0xFF;						//'0' on the display instead of nothing, we check for 'E' and set the output high
		} else if (ram_data[active_segment] <= '9' && ram_data[active_segment] >= '0') {
			PORTA = lktable[atoi(&ram_data[active_segment])];
		}
	}
}