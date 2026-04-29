#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "uart.h"

uint8_t           ring[RING_SIZE];
volatile uint16_t ring_head;
volatile uint16_t ring_tail;

/*
 * Clamp baud to the set of values for which calc_ubrr gives a
 * whole-number result within USART tolerance at 16 MHz / U2X.
 * Any value not in this list is replaced with 115200.
 * This prevents a garbage UBRR from producing an unresponsive UART.
 */
static uint32_t sanitize_baud(uint32_t baud)
{
	switch (baud) {
	case 9600:
	case 19200:
	case 38400:
	case 57600:
	case 115200:
		return baud;
	default:
		return 115200UL;
	}
}

/*
 * Compute UBRR register value for U2X mode.
 * Formula: UBRR = (F_CPU / (8 * baud)) - 1
 * U2X halves the divisor relative to normal mode, improving accuracy
 * for baud rates that do not divide evenly into F_CPU.
 */
static uint16_t calc_ubrr(uint32_t baud)
{
	return (uint16_t)((F_CPU / (8UL * baud)) - 1UL);
}

void uart_init(uint32_t baud)
{
	uint16_t ubrr;

	baud = sanitize_baud(baud);
	ubrr = calc_ubrr(baud);

	/* U2X: double the USART clock, halves the minimum baud error */
	UCSR0A = (1 << U2X0);

	/* baud rate registers — high byte first per datasheet */
	UBRR0H = (uint8_t)(ubrr >> 8);
	UBRR0L = (uint8_t)(ubrr & 0xFF);

	/* enable receiver, transmitter, and RX-complete interrupt */
	UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);

	/* frame format: 8 data bits, 1 stop bit, no parity */
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

/*
 * RX complete interrupt.
 * Fires on every received byte while RXCIE0 is set.
 * Reads UDR0 immediately — mandatory to clear RXC0 and prevent
 * a framing error on the next byte.
 *
 * If the ring is full (next == ring_tail), the byte is dropped.
 * The LED still toggles on drop so the operator can observe overrun
 * as an irregular blink pattern rather than silence.
 *
 * LED toggle: writing a 1 to PINx bit toggles the corresponding PORTx
 * bit without a read-modify-write. Single sbi instruction, safe in ISR.
 */
ISR(USART_RX_vect)
{
	uint8_t  byte = UDR0;
	uint16_t next = (ring_head + 1) & RING_MASK;

	if (next != ring_tail) {
		ring[ring_head] = byte;
		ring_head       = next;
	}

	PIND = (1 << PD5); /* toggle STAT LED — works even on overflow */
}

void uart_putc(uint8_t c)
{
	/* spin until the transmit data register is empty */
	while (!(UCSR0A & (1 << UDRE0)))
		;
	UDR0 = c;
}

void uart_puts_P(const char *s)
{
	uint8_t c;

	while ((c = pgm_read_byte(s++)) != 0)
		uart_putc(c);
}
