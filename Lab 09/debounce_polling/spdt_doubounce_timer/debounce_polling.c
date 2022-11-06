/*
 * debounce_polling.c
 *
 * Created: 12/13/2020 7:47:00 PM
 * Author : S. Michail
 *
 * A switch/button debouncer, suitable for both SPDT 
 * and SPST switches, using interrupt-based
 * polling of the input port. Works by detecting 
 * the rising or falling edge of the input signal.
 *
 * Assuming a clock of 1 MHz.
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sfr_defs.h>
#include <util/atomic.h>

#define HIGH 1
#define LOW 0

#define TRUE 1
#define FALSE 0

#define MAX_SAMPLES 10						//10 * .5ms polling => The signal is considered stable
											//if there is no bouncing for 5 ms
volatile unsigned char samples;
volatile unsigned char edge_detected;
volatile unsigned char switch_state;

/*
 * Intiialization functions
 * - Initializing I/O
 * - Initializing 8-bit timer0 (used for polling input)
 */
void
init_timer0(void)
{
	ATOMIC_BLOCK(ATOMIC_FORCEON) {
		TCCR0 |= (1 << CS02);				//Prescaler: f_clk/256 => 1MHz/256 => T_timer0 ~= 0.25 ms
		TIMSK |= (1 << TOIE0);				//Enable overflow interrupt
		TCNT0 = 254;						//2 * ~.25ms period => switch sample every .5ms 
	}
}

void
init_io(void)
{
	DDRA |= (1 << PINA2);					//PIN A 2 is an output
	PORTA |= (1 << PINA0) | (1 << PINA1);	//Pull-up resistors are high for PA0 and PA1 inputs (A and A')
}

/*
 * function: get_switch_state
 * description: returns HIGH or LOW depending on the raw
 * (non-debounced) switch state.
 *
 * args: void
 * returns: HIGH or LOW
 */
unsigned char 
get_switch_state(void)
{
	if (bit_is_set(PINA, PA0)) {
		return HIGH;			
	} else {
		return LOW;				
	}
}

int
main(void)
{
	init_timer0();
	init_io();
	sei();					//Initialize all necessary values
	
	samples = 0;			//Assuming the toggle switch is stable at the time
	edge_detected = FALSE;	//of device boot
	
	for (;;) {
		if (edge_detected) {				//If an edge has been successfully detected
			if (bit_is_set(PINA, PA0)) {	//and the switch state is considered stable
				PORTA |= (1 << PA2);		//then we can output that switch state
			} else {
				PORTA &=~ (1 << PA2);
			}
			edge_detected = FALSE;
		}
	}
}

/*
 * timer0 overflow interrupt handler
 *
 * The edge detection of the input signal is implemented by
 * polling the input every .5 milliseconds. If the current
 * state of the input is the same as the previous one for
 * MAX_SAMPLES in a row, then the input is considered stabilised
 * and the edge_detected flag is raised.
 *
 */
ISR(TIMER0_OVF_vect)
{
	if (switch_state == get_switch_state()) {
		samples++;		//new switch state is the same as the previous one
	} else {	
		samples = 0;	//if it's different, we reset the number of stable samples we have
	}
	
	switch_state = get_switch_state();
	
	if (samples == MAX_SAMPLES) {
		edge_detected = TRUE;		//If we have 2 ms of stability, we have successfully detected
	}								
	
	init_timer0();
}