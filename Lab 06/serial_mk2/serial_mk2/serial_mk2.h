/*
 * serial_mk2.h
 *
 *	Created: 18/10/2020 2:00:00 PM
 *  Author: S. Michail
 *
 *	Libraries and definitions needed
 *	for the serial_c program. 
 */ 

#ifndef SERIAL_MK2_H_
#define SERIAL_MK2_H_

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/sfr_defs.h>
#include <util/atomic.h>
#include <string.h>
#include <stdlib.h>

/*
 * "BAUD_PRESCALE" is the UBRRL value 
 * for the F_CPU and baud rate desired.
 * Since the STK500 on-board software 
 * clock is used for now, this is not needed,
 * and thus, is commented out.
 *
 *	#define F_CPU 3864000UL
 *	#define BAUD 19200
 *	#define BAUD_PRESCALE (((F_CPU / (BAUD * 16UL))) - 1)
 */

/*
 * Buffer size for the data received through
 * UART.
 */
#define BUFFER_SIZE 11

/*
 *	Used to implement boolean-like functions.
 */
#define TRUE 1
#define FALSE 0

/* 
 * Characters and strings that are
 * frequently used
 */
#define CHAR_NEWLINE '\n'
#define CHAR_RETURN '\r'
#define RETURN_NEWLINE "\r\n"
#define REPLY_OK "OK\r\n"
#define REPLY_FAILURE "ERROR\r\n"
#define REPLY_RESET "RESET\r\n"

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
volatile char rx_complete;
volatile int rx_n;
volatile int rx_len;
volatile char rx[BUFFER_SIZE];

/*
 * The data for the "N<X>\r\n" command is stored
 * in the RAM. To preserve the data through soft
 * resets, we place it in the "noinit" section of
 * the RAM, which does not initialize the data 
 * on every reset that is not due to power loss
 * or through the power switch.
 */
char ram_data[8] __attribute__ ((section (".noinit")));

/*
 * In case of a hard reset (any hard reset case, such as brown-out
 * reset, PWR reset or an external reset through the on-board
 * reset pins), we need to initialize the "noinit" RAM section 
 * which keeps the data intact in case of a soft reset. To do
 * that, we need the upper and lower limit of the noinit section.
 */
extern uint8_t __noinit_start, __noinit_end;

/*
 * The reset detection has to take place immediately after a reset,
 * and, as it sends a message back to the PC, the UART initialization
 * has to take place before the reset detection. After those are done,
 * the watchdog timer initialization also takes place before the 
 * main has executed, since it has to take place every time after a
 * reset.
 */
void init_uart(void) __attribute__ ((naked)) __attribute__((section(".init2")));
void detect_reset_source(void) __attribute__ ((naked)) __attribute__((section(".init3")));
void init_wdt(void) __attribute__ ((naked)) __attribute__((section(".init5")));

#endif /* SERIAL_MK2_H_ */