/*
 * serial_display_c.c
 *
 * Created: October 29th, 2020
 * Author : S. Michail
 * 
 *	Description
 */ 
#include <avr/io.h>
#include <avr/interrupt.h>

volatile unsigned char disp_buffer[8];
const unsigned char lktable[11] = {0b11000000,0b11111001,0b10100100,0b10110000,0b10011001,0b10010010,0b10000010,0b11111000,0b10000000,0b10010000,0b11111111};
volatile unsigned char digit = 0;

extern void timer0_handler(void);
extern char mask_ascii(char);

void init_output(void)
{
	DDRA = 0xFF;			//Set PORTA as output
	DDRC = 0xFF;			//Set PORTA as output
	PORTA = 0xFF;			//Default: all LEDs off
	PORTC = 0b11111110;		//as we have CA LEDs
}

void init_timer0(void)
{
	TIMSK = (1<<TOIE0);				//Enabling timer0 OVF interrupt
	TCCR0 = (1<<CS00)|(1<<CS02);	//f_clk/1024 prescaler setting
	TCNT0 = 252;					//256 - 252 = 4 * 1ms cycle = 4ms timer
}

void init_uart(void)
{
	UCSRB = (1<<TXEN)|(1<<RXEN)|(1<<RXCIE);		//Enable transmit, receive and receive complete interrupt
	UCSRC = (1<<UCSZ1)|(1<<UCSZ0)|(1<<URSEL);	//1 stop bit, no parity, and write to UBRRL
	UBRRL = 0x1A;								//Value for 2400 baud
}

void uart_send(unsigned char ch)
{
	while (! (UCSRA & (1 << UDRE)));	//Wait until UDR is empty
	UDR = ch;							//transmit the character
}

void reply(void)
{
	unsigned char i = 0;
	unsigned char strl = 4;
	unsigned char str[4] = "OK\r\n";
	
	while (i < strl)
	{
		uart_send(str[i++]);
	}
}

void reply_mem(void)
{
	unsigned char i = 0;
	unsigned char buffsz = 8;
	
	while (i < buffsz)
	{
		uart_send(disp_buffer[i++]);
	}
	uart_send('\r');
	uart_send('\n');
}

void reply_error(void)
{
	unsigned char i = 0;
	unsigned char strl = 7;
	unsigned char str[7] = "ERROR\r\n";
	
	while (i < strl) 
	{
		uart_send(str[i++]);
	}
}

void clear_buffer(void)
{
	unsigned char i = 0;
	
	while (i < 8) {
		disp_buffer[i++] = 'E';
	}
}

ISR(USART_RXC_vect)
{
	unsigned char ch;
	unsigned char i = 0;			//Used to count how many numbers we have stored
									//which is currently 8
	ch = UDR;						//get the character that was just received
	
	if (ch == 'A') {				//'AT': only reply to the PC
		goto receive_at;
	} else if (ch == 'C') {			//'C': clear the display and its memory
		clear_buffer();
		goto receive_cr;
	} else if (ch == 'N') {
		goto receive_number;		//'N<X>': store and display a new number
	} else if (ch == 'D') {			//'D': Display the number stored in memory, used for debugging
		reply_mem();
		reti();
	} else {
		reply_error();				//If no known command was enter, return error
		reti();						//and return from the interrupt
	}
	
receive_at:
	while (! (UCSRA & (1 << RXC)));		//wait until new data
	ch = UDR;
	if (ch == 'T')						//if we encounter anything other than 'T'
	{									//after 'A', it's not a valid command
		goto receive_cr;
	} else {
		reply_error();
		reti();
	}

receive_cr:
	while (! (UCSRA & (1 << RXC)));		//wait until new data
	ch = UDR;
	if (ch == '\r') {
		goto receive_end;				//<CR>: go to the end sink state
	} else {
		reply_error();
		reti();
	}

receive_number:
	while (! (UCSRA & (1 << RXC)));		//wait until new data
	ch = UDR;
	if (ch == '\r')						//return from the interrupt if we encounter
	{									//carriage return
		while (i < 7)
		{
			disp_buffer[i++] = 'E';		//add E to mark the empty remaining spaces of the display memory
		}								
		goto receive_end;
	} else if (ch >= '0' && ch <= '9') {
		if (i > 7)
		{
			clear_buffer();				//Empty the buffer
			reply_error();				//if we have more numbers than 8
			reti();						//return error and return from the interrupt
		} else { 
			disp_buffer[i++] = ch;		//store the numbers 
			goto receive_number;
		}
	} else {
		reply_error();
		reti();
	}
	
receive_end:							//End sink state
	while (! (UCSRA & (1 << RXC)));		//wait until new data
	ch = UDR;							//If the character is <LF>
	if (ch == '\n')						//the command has been successfully
	{									//received and processed
		reply();						//the reply is 'OK' and the interrupt
		reti();							//returns
	}
	else
	{
		reply_error();				//if the final character is not <LF>
		reti();						//the reply is 'ERROR' and the interrupt
	}								//returns
}

ISR(TIMER0_OVF_vect)
{
	unsigned char led_out;
	
	timer0_handler();
	if (disp_buffer[digit] == 'E') {
		led_out = 10;									//If 'E' is encountered
		} else {										//the LEDs are turned off
		led_out = mask_ascii(disp_buffer[digit]);				//Mask the upper 4 bits through assembly
	}
	PORTA = lktable[led_out];							//Send the output to the LEDs
	digit++;
	if (digit > 7) {
		digit = 0;
	}
}

int main(void) 
{
	init_output();
	init_uart();
	init_timer0();
	sei();				//Enable interrupts globally
		
    for (;;) {}			//for(;;) is "cheaper" than while(1)
}