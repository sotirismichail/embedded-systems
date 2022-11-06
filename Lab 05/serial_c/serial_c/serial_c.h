/*
 * serial_c.h
 *
 *	Created: 11/10/2020 2:12:31 PM
 *  Author: S. Michail
 *
 *	Libraries and definitions needed
 *	for the serial_c program. 
 */ 

#ifndef SERIAL_C_H_
#define SERIAL_C_H_

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <string.h>
#include <stdlib.h>

/*
 * "BAUD_PRESCALE" is the UBRRL value 
 * for the F_CPU and baud rate desired.
 * Since we are using the STK500 on-board
 * software clock, for now, this is not needed,
 * and thus, commented out.
 */
 
#define F_CPU 3864000UL
#define BAUD 19200
#define BAUD_PRESCALE (((F_CPU / (BAUD * 16UL))) - 1)

/*
 *	Used to implement boolean-like functions.
 */
#define TRUE 1
#define FALSE 0

/* 
 * Characters and strings that will be
 * frequently used
 */
#define CHAR_NEWLINE '\n'
#define CHAR_RETURN '\r'
#define RETURN_NEWLINE "\r\n"
#define REPLY_OK "OK\r\n"
#define REPLY_FAILURE "ERROR\r\n"

/*
 *	Lookup table for the seven segment codes.
 *	Assuming the output LEDs of the seven
 *	segment displays are of CA type
 *	(active low) 
 */
const unsigned char lktable[10] = {0b11000000,0b11111001,0b10100100,0b10110000,0b10011001,0b10010010,0b10000010,0b11111000,0b10000000,0b10010000};

/*
 *	Volatile is used for variables that will
 *	be used by the interrupt handlers (ISRs).
 */
volatile unsigned char active_segment;

/*
 * The data for the "N<X>\r\n" command is stored
 * in the RAM, specifically, beginning at the address
 *	0x006B of the AVR RAM.
 */
unsigned char *ram_data = (unsigned char*) 0x006B;

#endif /* SERIAL_C_H_ */