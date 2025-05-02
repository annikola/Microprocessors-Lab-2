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

// Interrupt Service Routine for UART receive
void uart_rx_isr(uint8_t rx) {
	// Check if the received character is a printable ASCII character
	if (rx >= 0x0 && rx <= 0x7F ) {
		// Store the received character
		queue_enqueue(&rx_queue, rx);
	}
}

void timer_isr() {
	ti_flag = 1;
}


int main() {
	
	int current_digit;
		
	// Variables to help with UART read / write
	uint8_t rx_char = 0;
	char buff[BUFF_SIZE]; // The UART read string will be stored here
	uint32_t buff_index;
	char diplay_message[50];
	
	// Initialize the receive queue and UART
	queue_init(&rx_queue, 128);
	uart_init(115200);
	uart_set_rx_callback(uart_rx_isr); // Set the UART receive callback function
	uart_enable(); // Enable UART module
	
	__enable_irq(); // Enable interrupts
	
	uart_print("\r\n");// Print newline
	
	// Initialize an interrupt timer
	timer_init(500000);
	timer_set_callback(timer_isr);
	timer_enable();
	
	// Set GPIO mode
	gpio_set_mode(P_LED_B, Output); // Set onboard LED pin to output
	// gpio_set_mode(P_SW, PullUp); // Switch pin to resistive pull-up
	
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
		
		// Main code here
		int cnt = 0;
		do {
			// Wait until a character is received in the queue
			__WFI(); // Wait for Interrupt
			if (ti_flag) {
				if (buff[cnt] == '-') {
					cnt = 0;
				}
				
				if (buff[cnt] % 2) {
					gpio_set(P_LED_B, 1);
					sprintf(diplay_message, "Digit %c -> Toggle LED\r\n", buff[cnt]);
				} else {
					gpio_set(P_LED_B, 0);
					sprintf(diplay_message, "Digit %c -> Blink LED\r\n", buff[cnt]);
				}
				uart_print(diplay_message);
				ti_flag = 0;
				cnt++;
			} else {
				uart_print("...\r\n(New input received)\r\n");
				break;
			}
			
		} while (cnt != buff_index - 1); // Continue until Enter key
		uart_print("\r\n"); // Print newline
	}
}
