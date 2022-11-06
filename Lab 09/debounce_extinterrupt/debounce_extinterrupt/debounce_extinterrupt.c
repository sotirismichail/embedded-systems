/*
 * debounce_extinterrupt.c
 *
 * Created: 12/13/2020 9:27:00 PM
 * Author : S. Michail
 *
 * A switch/button debouncer, suitable for both SPDT
 * and SPST switches, using external interrupts sensing
 * changes of the input port.
 *
 * Assuming a clock of 1 MHz
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
volatile unsigned char true_edge_detected;
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
	DDRD = 0x00;							//DDRD is input
}

int
main(void)
{
	init_timer0();
	init_io();
	GICR |= (1 << INT0);	//Enable external interrupt on INT0 pin
	MCUCR |= (1 << ISC00);	//Trigger the interrupt on any logical change of the INT0 pin signal
	sei();					//Initialize all necessary values
	
	samples = 0;				//Assuming the toggle switch is stable at the time
	true_edge_detected = FALSE;	//of device boot
	edge_detected = FALSE;
	
	for (;;) {
		if (true_edge_detected) {			//If an edge has been successfully detected
			if (bit_is_set(PIND, PD2)) {	//and the switch state is considered stable
				PORTA |= (1 << PA2);		//then we can output that switch state
			} else {
				PORTA &=~ (1 << PA2);
			}
			true_edge_detected = FALSE;
		}
	}
}

/*
 * External interrupt service routine
 * for the INT0 pin (to which A is connected)
 * 
 * Raises a flag every time there's any logical change 
 * to the input signal.
 */
ISR(INT0_vect)
{
	edge_detected = TRUE;
}

/*
 * timer0 overflow interrupt handler
 *
 * The edge detection of the input signal is implemented by
 * counting how many stable samples of the signal we retrieve. 
 * If we get MAX_SAMPLES of stable samples in a row, 
 * then the input is considered stabilised
 * and the true_edge_detected flag is raised.
 *
 */
ISR(TIMER0_OVF_vect)
{	
	if (edge_detected) {
		samples = 0;		//new switch state is the same as the previous one
		edge_detected = FALSE;
	} else {
		samples++;
	}
	
	if (samples == MAX_SAMPLES) {
		true_edge_detected = TRUE;		//If we have 2 ms of stability, we have successfully detected
	}								
	
	init_timer0();
}
