/*
 * matrix_mult.c
 *
 * Created: 12/18/2020 8:47:47 PM
 * Author : S. Michail
 *
 * Description: Multiplying 2 3x3 matrices of floating
 *				point or integer elements on an AVR
 *				microcontroller.
 */ 
#include <avr/io.h>
#include <stdlib.h>

/*
 * Matrix element type
 * Switch to "int" or "float" to perform
 * integer or floating point matrix arithmetic.
 */
typedef int element_t;

/*
 * Declaring the 3x3 matrices used, a and b are the multiplication
 * operands, c is the result. The matrices are stored in the .data
 * section of the SRAM. Specific addresses can be found in the inc-
 * luded memory map. The "volatile" attribute is used so as to avoid
 * compiler optimizations removing the variables as they can be deemed
 * unused.
 */
volatile element_t a[3][3] __attribute__ ((section(".data")));
volatile element_t b[3][3] __attribute__ ((section(".data")));
volatile element_t c[3][3] __attribute__ ((section(".data")));

/*
 * function: matrix_init
 *
 * args: matrix (3x3)
 *
 * description: Fills a 3x3 matrix with (pseudo) random
 *				elements
 */
void
matrix_init(element_t matrix[3][3])
{
	for (unsigned char i = 0; i <= 2; i++) {
		for (unsigned char j = 0; j <= 2; j++) {
			matrix[i][j] = (element_t)rand();
		}
	}
}

/*
 * function: matrix_mult
 *
 * args: matrix a, matrix b, matrix a*b (3x3)
 *
 * description: Given the memory addresses of three matrices, the
 *				matrices a and b are multiplied, and the result is
 *				another 3x3 matrix, which is stored as matrix "c".
 */
void
matrix_mult(element_t a[3][3], element_t b[3][3], element_t c[3][3]) 
{
	element_t sum;

	for (unsigned char i = 0; i <= 2; i++) {
		for (unsigned char j = 0; j <= 2; j++) {
			sum = 0;
			for (unsigned char k = 0; k <= 2; k++) {
				sum = sum + a[i][k] * b[k][j];
			}
			c[i][j] = sum;
		}
	}
}

int
main(void) 
{
	srand(0);					//Initialize the pseudorandom generator
	
	matrix_init(a);		//Load random numbers in the two matrices
	matrix_init(b);

	matrix_mult(a,b,c);			//Perform the multiplication
}

