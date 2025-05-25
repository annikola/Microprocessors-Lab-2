#include "platform.h"
#include <stdio.h>
#include <stdint.h>
#include "uart.h"
#include <string.h>
#include "queue.h"
#include "gpio.h"
#include "timer.h"


/*
*--------------------- 2nd project in Microprocessors -----------------------*

Consists of two parts:
- The UART read-write code
- The interrupt based functionality


	The user inputs a number (characters are ignored except "-"), when enter is pressed ("\r")
	the analysis of the number begins, each number is processed every 0.5 seconds and 
	- if odd, the LED toggles
	- if even, the led blinks with a half period of 200ms
	- if "-", starts over

	If the buttons is pressed the LED freezes and everything continues as normal.
	If it is pressed again the LED unfreezes.
	The total button presses are counted and printed every time it is pressed.

	If the number ends or a new key is pressed the process start over immediately.

The pins of the LED and the button are defined in the platform.h file and are
stored in the variables:
P_LED_R, P_SW  


For the reading of the characters every 0.5sec the SysTick timer interupt is used.
For the LED blinking the RCC_APB1Periph_TIM2 timer is used.


When the stage is that of character input, the button presses do nothing
because there is no analysis happening. 
But the button pressed is counted on the overal amount.
A variable named frozen indicates if the LED is frozen, and a variable 
named input_phase indicates if we are currently on the character input phase.


The action happens inside the interrupt's ISRs.


The priorities are acounted so that the button interrupt is above everything,
then the keyboard interrupt and then the SysTick and the TIM2.
This way if the button is pressed, the LED freezes before its state is changed
and if a key is pressed during the analysis, the analysis stops immediately.

Other files that also have changes are:
	- gpio.c	(lines 259 and 348)
	- uart.c	(line 73)
	- timer.c (line 18)
*/





/*         UART variable definitions         */
#define BUFF_SIZE 128 //read buffer length

Queue rx_queue;       // Queue for storing received characters
char buff[BUFF_SIZE]; // The UART read string will be stored here
int current_digit = 0;          // The index of the digit being analysed
int input_phase = 1;  // input_phase = 1 if we are at the stage of inputing numbers



/*       Button variable definitions         */
int frozen = 0;       // frozen = 1 if the button has been pressed an odd amount of times
unsigned int button_press_count = 0;   // amount of button presses since the last reset 



/*       Interrupt Service Routine for UART receive       */
void uart_rx_isr(uint8_t rx) {
	// Check if the received character is a printable ASCII character
	if (rx >= 0x0 && rx <= 0x7F ) {
		// Store the received character
		queue_enqueue(&rx_queue, rx);
	}
}


/*      Interrupt Sevice Routine for analysing characters      */
//       using SysTick
void digit_timer_isr() {
	// analyses the characted in the buffer given by index current_digit
	// every 0.5sec
	
	char display_message[32];   // string holding the message to print (max 32 chars)
	
	if (buff[current_digit] == '-') {
		// if the character is '-' start from the beginning (first character)
		current_digit = 0;
	}
	
	if (buff[current_digit] % 2) {
		// the number is odd, toggle the LED
		// but only if the LED is not frozen (button pressed an odd amount)
		
		if (!frozen) {
			// Disable the TIM2 timer to stop the LED blinking
			// Also clear pending interrupts that could be wainting to occur
			NVIC_ClearPendingIRQ(TIM2_IRQn);  
			NVIC_DisableIRQ(TIM2_IRQn);     
			
			gpio_toggle(P_LED_R);         // toggle the LED
			sprintf(display_message, "Digit %c -> Toggle LED\r\n", buff[current_digit]);
		} else {
			// button has been pressed, LED is frozen
			sprintf(display_message, "Digit %c -> Skipped LED action\r\n", buff[current_digit]);
		}	
	} else {
		// the number is even, start the TIM2 timer
		// but only if the LED is not frozen
		if (!frozen) {
			NVIC_EnableIRQ(TIM2_IRQn);   // Eneble TIM2
			TIM2->CR1 |= TIM_CR1_CEN;	   // Start TIM2
			sprintf(display_message, "Digit %c -> Blink LED\r\n", buff[current_digit]);
		} else {
			// LED is frozen
			sprintf(display_message, "Digit %c -> Skipped LED action\r\n", buff[current_digit]);
		}
	}
	uart_print(display_message);
	current_digit++;     // go to next number
}


/*      Interrupt Sevice Routine for LED blinking      */
void TIM2_IRQHandler(void) {
	// toggle the LED every 200ms
	
	if (TIM2->SR & TIM_SR_UIF) {	// Check if update interrupt flag is set
		TIM2->SR &= ~TIM_SR_UIF;		// Clear the flag immediately
		gpio_toggle(P_LED_R);
	}
}


/*      Interrupt Sevice Routine for button press      */
void freeze(int status) {
	char display_message[60];  // string holding the message to print (max 60 chars)
	
	// button has been pressed! add one to the count
	button_press_count++;
	
	if (!input_phase) {
		// we are not on the character input stage
		
		NVIC_DisableIRQ(TIM2_IRQn);    // stop the LED timer
		frozen = !frozen;              // toggle the frozen variable
		sprintf(display_message, "Interrupt: Button pressed. LED locked. Count = %d\r\n", button_press_count);
		uart_print(display_message);
	}
}


int main() {
	
	// Variables to help with UART read / write
	uint8_t rx_char = 0;
	uint32_t buff_index;
	
	// Initialize the receive queue and UART
	queue_init(&rx_queue, 128);
	uart_init(115200);
	uart_set_rx_callback(uart_rx_isr); // Set the UART receive callback function
	uart_enable(); // Enable UART module
	
	__enable_irq(); // Enable interrupts
	
	uart_print("\r\n");// Print newline
	
	
	// Initialize the led interrupt timer
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;  // Enable clock for TIM2
	// set the amount of ticks relative to the clock speed
	TIM2->PSC = 15999;  // Prescaler: divide 16 MHz by 16000 = 1 kHz
	TIM2->ARR = 199;   // Auto-reload: 200 ticks at 1 kHz = 200 ms
	TIM2->DIER |= TIM_DIER_UIE;     // Enable update interrupt (overflow interrupt)
	NVIC_SetPriority(TIM2_IRQn, 3);  // set the priority 
	
	// Initialize LEDs
	gpio_set_mode(P_LED_R, Output); // Set onboard LED pin to output
	gpio_set(P_LED_R, LED_OFF);     // initialise to off
	
	// Initialize the Push Button (User Button)
	gpio_set_mode(P_SW, PullUp);     // St pin to resistive pull-up mode
	gpio_set_trigger(P_SW, Falling); // trigger (call interupt) on falling edge
	gpio_set_callback(P_SW, freeze); // Set the Push Button ISR function
	
	
	
	
	while(1) {

		// Prompt the user to enter a digit sequence
		uart_print("Input: ");
		buff_index = 0; // Reset buffer index
		
		do {
			// Wait until a digit or dash is received in the queue
			while (!queue_dequeue(&rx_queue, &rx_char))
				__WFI(); // Wait for Interrupt

			if (rx_char == 0x7F) { // Handle backspace character
				if (buff_index > 0) {
					buff_index--; // Move buffer index back
					uart_tx(rx_char); // Send backspace character to erase on terminal
				}
			} else if ((rx_char >= '0' && rx_char <= '9') || rx_char == '-' || rx_char == '\r') {
				// take into acound only numbers, '-' and '\r', ignore everything else
				// Store and echo the received character back
				buff[buff_index++] = (char)rx_char; // Store digit or dash in buffer
				uart_tx(rx_char); // Echo digit or dash back to terminal
			}
		} while (rx_char != '\r' && buff_index < BUFF_SIZE); // Continue until Enter key or buffer full
		
		// Replace the last character with null terminator to make it a valid C string
		buff[buff_index - 1] = '\0';
		uart_print("\r\n"); // Print newline
		
		// Check if buffer overflow occurred
		if (buff_index >= BUFF_SIZE) {
			uart_print("Stop trying to overflow my buffer! I resent that!\r\n");
		}
		
		// Sequence processing
		input_phase = 0;             // exited input stage
		current_digit = 0;           // starting to analyse from first character
		
		// initialise character timer (SysTick 0.5 sec period)
		timer_init(500000);          // 500'000 µs = 0.5 s
		timer_set_callback(digit_timer_isr); // set the callback function to the one defined above
		timer_enable();              // enable
		
		while (current_digit != buff_index - 1) { 
			// do until currect character position (current_digit) reaches the end of the buffer
			// don't account for the last position as it is '\0'
			
			__WFI(); // Wait for Interrupt
			
			if (!queue_is_empty(&rx_queue)) {
				// queue is not empty, the interupt was a key press, exit the loop to start over..
				uart_print("...\r\n(New input received)\r\n");
				break;
			}
		}
		
		// the whole number has been processed, or a button was pressed
		// prepare for next number
		NVIC_DisableIRQ(TIM2_IRQn);    // disable the LED blinking timer
		timer_disable();               // disable the character timer
		gpio_set(P_LED_R, 0);          // set the LED to off
		frozen = 0;                    // unfreeze
		input_phase = 1;               // enter input stage
		
		if (current_digit == buff_index - 1) {
			// if current position is the last position of the buffer, the whole sequence was proccesed
			uart_print("End of sequence. Waiting for new number...\r\n");
		}
	}
}
