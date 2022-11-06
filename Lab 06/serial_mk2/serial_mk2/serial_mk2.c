/*
 *	serial_mk2.c
 *
 *	Created: 18/10/2020 2:00:00 PM
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

#include "serial_mk2.h"

/*
 * Initialization functions (executed in this order)
 * - UART interface, setting two-way communication in the correct baud rate (executed before main)
 * - Watchdog timer															(executed before main)
 * - LED display
 * - timer0, for the display refresh
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
	UBRRL = 11;									//Value for 19200 baud
}	

void
init_wdt()
{														//Enable the watchdog timer
	WDTCR |= (1<<WDE)|(1<<WDP2)|(1<<WDP1);				//WDP2..0 = 110, ~1s reset
}

/*
 * function: detect_reset_source
 * description: Checking the MCUCSR register, we can detect what
 *				kind of reset has occurred. If a watchdog timer soft
 *				reset has occurred, the WDRF flag of the register is
 *				raised, and a message is sent to the PC to signal that
 *				a soft reset has taken place. In case of any other reset,
 *				the .noinit part of the RAM gets reinitialized, so we
 *				have a cold start.
 *
 *				This function gets executed before main() and right after
 *				the UART interface is initialized, so the message transmission
 *				is successful. See the header file for further details.
 *
 * arg0: void
 *
 * returns: void
 */
void
detect_reset_source()
{
	if (bit_is_set(MCUCSR, WDRF)) {
		uart_puts(REPLY_RESET);					//If there's a watchdog soft-reset, send a message to the PC
	} else {									//PWR or other reset, reinitialize noinit RAM
		for (uint8_t *p = &__noinit_start; p < &__noinit_end; p++) {
			*p = 0;
		}
	}
	MCUCSR = 0x00;								//Reset the MCUCSR register for the next reset
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
 *				 Because it's a critical point of the program, we have to block
 *				 any interrupts using atomic block.
 *				 
 *				 ram_buffer is a RAM location in the .noinit section of the
 *				 RAM, so a soft reset does not reinitialize it.
 *
 *	arg0: void
 *	
 *	returns: void
 */
unsigned char
set_buffer(char *str) 
{
ATOMIC_BLOCK(ATOMIC_FORCEON) {
	unsigned char retval, i = 1;
	
	while(i < 8 && str[i] != CHAR_RETURN) {
		ram_data[i - 1] = str[i];
		i++;
	}
	
	if (str[i] == CHAR_RETURN && str[i + 1] == CHAR_NEWLINE)
		retval = TRUE;
	else
		retval = FALSE;
		
	while (i < 8) {
		ram_data[i] = 'E';	//Fill the empty display buffer spaces with
		i++;				//10 which signifies empty if we have received
	}						//less than 8 numerals, to prevent trailing zeroes
	
	return retval;
	}
}

/*
 *	function: copy_command 
 *	description: Copies the contents of the receive (rx) buffer into another
 *				 buffer, which can be processed while receiving new characters.
 *				 Because it's a critical point of the program, we have to block
 *				 any interrupts using atomic block.
 *
 *	arg0: a string pointer
 *	
 *	returns: void
 */
void
copy_command(char* str) {
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		memcpy(str, rx, rx_len);
		memset(rx, 0, rx_len);
	}
}

/*
 *	function: process_command 
 *	description: Checks the contents of the command buffer and calls the appropriate
 *				 function that implements the command received.
 *
 *	arg0: a string pointer
 *	
 *	returns: void
 */
void
process_command(char *str) {
	switch (str[0]) {
		case 'A':
			uart_puts(REPLY_OK);
			break;
		case 'D':
			uart_puts(REPLY_OK);
			ret_buffer();
			break;
		case 'C':
			uart_puts(REPLY_OK);
			clr_buffer();
			break;
		case 'N':
			set_buffer(str);
			break;
		default:
			uart_puts(REPLY_FAILURE);
	}
}

/*
 *	function: (ISR) USART RX RECEIVE Interrupt Handler
 *	description: Triggered every time a character is received, the data
 *				 is stored in a buffer, which is later processed by the program.
 *				 The rx_receive flag is raised when we have received a complete
 *				 command, which ends with '\n'.
 */
ISR(USART_RXC_vect)
{
	if (rx_n == BUFFER_SIZE) {		//If we get more than 11 characters, which is 
		rx_n = 0;					//the longest command we have for now, reset
	}								//the buffer and start once more.
	rx[rx_n] = UDR;					//store the character from UDR in the buffer
	wdt_reset();					//reset the watchdog timer each time we receive a character
	if (rx[rx_n] == CHAR_NEWLINE)	//If we receive a newline, raise the rx_complete flag
	{								//store the length of the command so it can be copied
		rx_complete = TRUE;			//and reset the buffer index.
		rx_len = rx_n;
		rx_n = 0;
	} else {
		rx_n++;
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
 *				 (active low).
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
	init_timer0();					//Initialize timer0
	active_segment = 0;				//Initialize the active single digit display
	sei();							//Enable interrupts globally
	
	unsigned char command[BUFFER_SIZE];

    for (;;) 
    {
		/*
		 * Command processing
		 */ 
		if (rx_complete == TRUE) {				//if the rx_complete flag is raised, we have received a whole command
			copy_command(command);				//copy it into the command buffer
			process_command(command);			//and process the command
			rx_complete = FALSE;				//lower the command receive flag to receive another while the command is processing
		}
		/*
		 * Display driver
		 */
		if (ram_data[active_segment] == 'E') {	//'E' denotes an empty space in the RAM. To prevent showing
			PORTA = 0xFF;						//'0' on the display instead of nothing, we check for 'E' and set the output high
		} else if (ram_data[active_segment] <= '9' && ram_data[active_segment] >= '0') {
			PORTA = lktable[atoi(&ram_data[active_segment])];
		}
	}
}