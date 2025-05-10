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
int ti_flag = 0;
int frozen = 0;

// Interrupt Service Routine for UART receive
void uart_rx_isr(uint8_t rx) {
	// Check if the received character is a printable ASCII character
	if (rx >= 0x0 && rx <= 0x7F ) {
		// Store the received character
		queue_enqueue(&rx_queue, rx);
	}
}

void digit_timer_isr() {
	ti_flag = 1;
}

void TIM2_IRQHandler(void) {
	if (TIM2->SR & TIM_SR_UIF) {	// Check if update interrupt flag is set
		TIM2->SR &= ~TIM_SR_UIF;		// ? Clear the flag immediately

		// Your custom logic here — e.g. toggle LED, set flags
		gpio_toggle(P_LED_R);
	}
}


int main() {
	
	int current_digit;
		
	// Variables to help with UART read / write
	uint8_t rx_char = 0;
	char buff[BUFF_SIZE]; // The UART read string will be stored here
	uint32_t buff_index;
	char display_message[50];
	
	// Initialize the receive queue and UART
	queue_init(&rx_queue, 128);
	uart_init(115200);
	uart_set_rx_callback(uart_rx_isr); // Set the UART receive callback function
	uart_enable(); // Enable UART module
	
	__enable_irq(); // Enable interrupts
	
	uart_print("\r\n");// Print newline
	
	// Initialize the digit interrupt timer
	timer_init(1000000);
	timer_set_callback(digit_timer_isr);
	timer_enable();
	
	// Initialize the led interrupt timer
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;  // Enable clock for TIM2
	TIM2->PSC = 15999;  // Prescaler: divide 16 MHz by 16000 = 1 kHz
	TIM2->ARR = 199;   // Auto-reload: 200 ticks at 1 kHz = 200 ms
	TIM2->DIER |= TIM_DIER_UIE;  // Enable update interrupt (overflow interrupt)
	NVIC_SetPriority(TIM2_IRQn, 1);
	
	// Set GPIO mode
	gpio_set_mode(P_LED_R, Output); // Set onboard LED pin to output
	gpio_set_mode(P_SW, PullUp); // Switch pin to resistive pull-up
	gpio_set(P_LED_R, LED_OFF);
	
	while(1) {

		// Prompt the user to enter their full name
		uart_print("Input: ");
		buff_index = 0; // Reset buffer index
		
		do {
			// Wait until a character is received in the queue
			while (!queue_dequeue(&rx_queue, &rx_char))
				__WFI(); // Wait for Interrupt

			if (rx_char == 0x7F) { // Handle backspace character
				if (buff_index > 0) {
					buff_index--; // Move buffer index back
					uart_tx(rx_char); // Send backspace character to erase on terminal
				}
			} else if ((rx_char >= '0' && rx_char <= '9') || rx_char == '-' || rx_char == '\r') {
				// Store and echo the received character back
				buff[buff_index++] = (char)rx_char; // Store character in buffer
				uart_tx(rx_char); // Echo character back to terminal
			}
		} while (rx_char != '\r' && buff_index < BUFF_SIZE); // Continue until Enter key or buffer full
		
		// Replace the last character with null terminator to make it a valid C string
		buff[buff_index - 1] = '\0';
		uart_print("\r\n"); // Print newline
		
		// Check if buffer overflow occurred
		if (buff_index >= BUFF_SIZE) {
			uart_print("Stop trying to overflow my buffer! I resent that!\r\n");
		}
		
		// Main code
		int cnt = 0;
		do {
			
			__WFI(); // Wait for Interrupt
			
			if (!gpio_get(P_SW)) {
				NVIC_DisableIRQ(TIM2_IRQn);
				frozen = 1;
			}
			
			if (ti_flag) {
				if (buff[cnt] == '-') {
					cnt = 0;
				}
				
				if (buff[cnt] % 2) {
					if (!frozen) {
						NVIC_DisableIRQ(TIM2_IRQn);
						gpio_toggle(P_LED_R);
					}
					sprintf(display_message, "Digit %c -> Toggle LED\r\n", buff[cnt]);
				} else {
						if (!frozen) {
							NVIC_EnableIRQ(TIM2_IRQn);
							TIM2->CR1 |= TIM_CR1_CEN;	// Start TIM2
						}
					sprintf(display_message, "Digit %c -> Blink LED\r\n", buff[cnt]);
				}
				uart_print(display_message);
				ti_flag = 0;
				cnt++;
			} else if (!queue_is_empty(&rx_queue)) {
				NVIC_DisableIRQ(TIM2_IRQn);
				uart_print("...\r\n(New input received)\r\n");
				break;
			}
			
		} while (cnt != buff_index - 1); // Continue until Enter key
		gpio_set(P_LED_R, 0);
		frozen = 0;
		uart_print("\r\n"); // Print newline
	}
}
