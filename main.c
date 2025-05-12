#include "platform.h"
#include <stdio.h>
#include <stdint.h>
#include "uart.h"
#include <string.h>
#include "queue.h"
#include "gpio.h"
#include "timer.h"


#define BUFF_SIZE 128 //read buffer length

Queue rx_queue; // Queue for storing received characters
char buff[BUFF_SIZE]; // The UART read string will be stored here
int cnt = 0;
int frozen = 0;
int input_phase = 1;
unsigned int button_press_count = 0;

// Interrupt Service Routine for UART receive
void uart_rx_isr(uint8_t rx) {
	// Check if the received character is a printable ASCII character
	if (rx >= 0x0 && rx <= 0x7F ) {
		// Store the received character
		queue_enqueue(&rx_queue, rx);
	}
}

void digit_timer_isr() {
	
	char display_message[32];
	
	if (buff[cnt] == '-') {
		cnt = 0;
	}
	
	if (buff[cnt] % 2) {
		if (!frozen) {
			NVIC_ClearPendingIRQ(TIM2_IRQn);
			NVIC_DisableIRQ(TIM2_IRQn);
			gpio_toggle(P_LED_R);
			sprintf(display_message, "Digit %c -> Toggle LED\r\n", buff[cnt]);
		} else {
			sprintf(display_message, "Digit %c -> Skipped LED action\r\n", buff[cnt]);
		}	
	} else {
		if (!frozen) {
			NVIC_EnableIRQ(TIM2_IRQn);
			TIM2->CR1 |= TIM_CR1_CEN;	// Start TIM2
			sprintf(display_message, "Digit %c -> Blink LED\r\n", buff[cnt]);
		} else {
			sprintf(display_message, "Digit %c -> Skipped LED action\r\n", buff[cnt]);
		}
	}
	uart_print(display_message);
	cnt++;
}

void TIM2_IRQHandler(void) {
	if (TIM2->SR & TIM_SR_UIF) {	// Check if update interrupt flag is set
		TIM2->SR &= ~TIM_SR_UIF;		// Clear the flag immediately
		gpio_toggle(P_LED_R);
	}
}

void freeze(int status) {
	char display_message[60];
	
	button_press_count++;
	if (!input_phase) {
		NVIC_DisableIRQ(TIM2_IRQn);
		frozen = !frozen;
		sprintf(display_message, "Interrupt: Button pressed. LED locked. Count = %d\r\n", button_press_count);
		uart_print(display_message);
	}
}


int main() {
	
	int current_digit;
		
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
	TIM2->PSC = 15999;  // Prescaler: divide 16 MHz by 16000 = 1 kHz
	TIM2->ARR = 199;   // Auto-reload: 200 ticks at 1 kHz = 200 ms
	TIM2->DIER |= TIM_DIER_UIE;  // Enable update interrupt (overflow interrupt)
	NVIC_SetPriority(TIM2_IRQn, 1);
	
	// Initialize LEDs
	gpio_set_mode(P_LED_R, Output); // Set onboard LED pin to output
	gpio_set(P_LED_R, LED_OFF);
	
	// Initialize the Push Button (User Button)
	gpio_set_mode(P_SW, PullUp); // Switch pin to resistive pull-up
	gpio_set_trigger(P_SW, Falling);
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
		input_phase = 0;
		cnt = 0;
		timer_init(500000);
		timer_set_callback(digit_timer_isr);
		timer_enable();
		while (cnt != buff_index - 1) {
			
			__WFI(); // Wait for Interrupt
			
			if (!queue_is_empty(&rx_queue)) {
				uart_print("...\r\n(New input received)\r\n");
				break;
			}
		}
		NVIC_DisableIRQ(TIM2_IRQn);
		timer_disable();
		gpio_set(P_LED_R, 0);
		frozen = 0;
		input_phase = 1;
		if (cnt == buff_index - 1) {
			uart_print("End of sequence. Waiting for new number...\r\n");
		}
	}
}
