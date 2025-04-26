	.data
table:
    .word 5, 12, 7, 6, 4, 11, 6, 3, 10, 23	// Define the table of values

	.text
	.global		map
	.global		reduce
	.global     fibonacci
	.global     crc_like_checksum
	.p2align	2
	.type		map, %function
	.type		reduce, %function
	.type       fibonacci, %function
	.type       crc_like_checksum, %function
		

map:
	.fnstart
	mov		R1, R0		// R0 by convention holds the return value, therefore its current value (first argument) is transfered to R1
	mov		R0, #0
	ldr     R4, =table	// Load the address of 'table' into R4

loop:
	ldrb	R2, [R1], #1	// Load byte from R1, which is the currect character of the string and increament R1 by 1
	cmp		R2, #0			// If it is the null character ('\0')
	beq		done			// exit the function and return the result

capital:
	cmp		R2, #65			// If it is lower than 65 ('A')
	blt		number			// it could only be a number
	cmp		R2, #90			// If it is higher than 90 ('Z')
	bgt		small			// it could only be a small letter
	lsl		R3, R2, #1		// Bitwise sift to the left for a fast multiplication with 2
	add		R0, R0, R3		// Accumulate the result
	b		increament

small:
	cmp		R2, #97
	blt		increament
	cmp		R2, #122
	bgt		increament
	sub		R3, R2, #97				// Subtract 'a' (97) from the value of R2
	mla		R0, R3, R3, R0			// Multiply and accumulate the result
	b		increament

number:
	cmp		R2, #48
	blt		increament
	cmp		R2, #57
	bgt		increament
	sub		R2, R2, #48				// Standarize for index use
	lsl		R2, R2, #2				// word = 4 bytes
	ldr		R3, [R4, R2]	// Load the value for the number of the corresponding index (scale the index by the WORD size)
	add		R0, R0, R3				// Accumulate the result
	b		increament

increament:
	add		R0, R0, #1				// Add 1 for each character accessed to compute the length
	b		loop
	
done:
	bx		LR
	.fnend
	
reduce:
	.fnstart
	mov		R1, R0
	mov		R0, #0
	mov		R2, #10					// Base 10

adder:
	sdiv	R3, R1, R2				// Integer division to find the quotient (R3)
	mul		R4, R3, R2				// Multiply the divisor (R2) with the quotient (R3)
	sub		R4, R1, R4				// and subtract from the dividend
	add		R0, R0, R4				// This way the module is calculated and added to the result
	mov		R1, R3					// Keep only the quotient and repeat	
	cmp		R1, R2					// if it is greater than 10
	bge		adder
	add		R0, R0, R1				// If R1 is less than 10 this is the last digit to add

mod7:
	cmp		R0, #9
	ble		done2
	mov		R2, #7
	sdiv	R3, R0, R2
	mul		R4, R3, R2
	sub		R0, R0, R4
	
done2:
	bx		lr
	.fnend
	

	
	
fibonacci:
	.fnstart
	mov     r1, r0         // move input to r1
	cmp     r1, #0         // if input == 0 
	beq     fib_zero       // go to case zero
	cmp     r1, #1         // if input == 1
	beq     fib_one        // go to case 1
	                       // call fibonacci(n-1)
	push    {r1, lr}       // push input to stack 
	sub     r0, r1, #1     // make r0 (function input) be n - 1
	bl      fibonacci      // call fibonacci(n-1)
	pop     {r1, lr}       // pop from stack the original input of this call of the function
	mov     r2, r0         // move the output of fibonacci(n-1) to r2
	sub     r0, r1, #2     // make r0 be input - 2
	push    {r1, r2, lr}
	bl      fibonacci
	pop     {r1, r2, lr}
	mov     r3, r0         // r3 is the output of fibonacci(n-2)
	add     r0, r2, r3     // add them together and return the result
	b       fib_done
	
	
	
	
fib_zero:
	mov     r0, #0          // return zero 
	b       fib_done

	
fib_one:
	mov     r0, #1           // return one
	b       fib_done


fib_done:
	bx      lr
	.fnend
	
	

crc_like_checksum:
// takes a string and performs the bitwise xor of all characters with eachother, returns the result
	.fnstart
	mov     r1, r0           // move the string address to r1
	ldrb    r0, [r1], #1     // load on r0 (output) the first character, and increment the address to show the next character
	cmp     r0, #0           
	beq     char_xor_done    // if it is the null character, exit and return 
	
	
	
char_loop:
	ldrb    r2, [r1], #1     // load current character on r2 and increment r1 (string address) by 1
	cmp     r2, #0           
	beq     char_xor_done    // if it is the null character, exit and return 
	eor     r0, r0, r2       // else, perform bitwise xor of the current accumulative xor and the current character
	b       char_loop
	
	
char_xor_done:
	bx      lr
	.fnend
	