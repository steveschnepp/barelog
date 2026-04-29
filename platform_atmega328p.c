#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include "platform.h"

/* ------------------------------------------------------------------ */
/* ring buffer — shared by UART RX ISR                                */
/* ------------------------------------------------------------------ */

uint8_t           ring[RING_SIZE];
volatile uint16_t ring_head;
volatile uint16_t ring_tail;

/* ------------------------------------------------------------------ */
/* UART — init, TX, RX ISR                                            */
/* ------------------------------------------------------------------ */

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

static uint16_t calc_ubrr(uint32_t baud)
{
	return (uint16_t)((F_CPU / (8UL * baud)) - 1UL);
}

void uart_init(uint32_t baud)
{
	uint16_t ubrr;

	baud = sanitize_baud(baud);
	ubrr = calc_ubrr(baud);

	UCSR0A = (1 << U2X0);
	UBRR0H = (uint8_t)(ubrr >> 8);
	UBRR0L = (uint8_t)(ubrr & 0xFF);
	UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

ISR(USART_RX_vect)
{
	uint8_t  byte = UDR0;
	uint16_t next = (ring_head + 1) & RING_MASK;

	if (next != ring_tail) {
		ring[ring_head] = byte;
		ring_head       = next;
	}

	PIND = (1 << PD5);
}

void uart_putc(uint8_t c)
{
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

uint8_t uart_getc_poll(void)
{
	while (!(UCSR0A & (1 << RXC0)))
		;
	return UDR0;
}

/* ------------------------------------------------------------------ */
/* SPI — init, transfer, CS control, bulk I/O                         */
/* ------------------------------------------------------------------ */

void spi_init(void)
{
	SPI_DDR |=  (1 << SPI_CS) | (1 << SPI_MOSI) | (1 << SPI_SCK);
	SPI_DDR &= ~(1 << SPI_MISO);

	SPI_PORT |= (1 << SPI_CS) | (1 << SPI_MOSI);

	SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);
	SPSR &= ~(1 << SPI2X);
}

void spi_set_fast(void)
{
	SPCR &= ~((1 << SPR1) | (1 << SPR0));
	SPSR |=  (1 << SPI2X);
}

void spi_write_buf(const uint8_t *buf, uint16_t len)
{
	uint16_t i;

	for (i = 0; i < len; i++)
		spi_transfer(buf[i]);
}

void spi_write_zeros(uint16_t len)
{
	uint16_t i;

	for (i = 0; i < len; i++)
		spi_transfer(0x00);
}

void spi_read_buf(uint8_t *buf, uint16_t len)
{
	uint16_t i;

	for (i = 0; i < len; i++)
		buf[i] = spi_transfer(0xFF);
}

/* ------------------------------------------------------------------ */
/* Timer1 — 500 ms CTC for idle flush                                 */
/* ------------------------------------------------------------------ */

volatile static uint8_t timer_pending;

void timer_init(void)
{
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10);
	OCR1A  = 7812;
	TIMSK1 = (1 << OCIE1A);
	TCNT1  = 0;
	timer_pending = 0;
}

ISR(TIMER1_COMPA_vect)
{
	timer_pending = 1;
}

uint8_t timer_flush_pending(void)
{
	return timer_pending;
}

void timer_clear_pending(void)
{
	timer_pending = 0;
}

void timer_restart(void)
{
	TCNT1 = 0;
}

/* ------------------------------------------------------------------ */
/* EEPROM — read, write, update                                       */
/* ------------------------------------------------------------------ */

void eeprom_init(void)
{
	/* nothing needed on AVR */
}

uint8_t eeprom_read_byte(uint16_t addr)
{
	return eeprom_read_byte((uint8_t *)addr);
}

void eeprom_write_byte(uint16_t addr, uint8_t val)
{
	eeprom_write_byte((uint8_t *)addr, val);
}

void eeprom_update_byte(uint16_t addr, uint8_t val)
{
	eeprom_update_byte((uint8_t *)addr, val);
}

/* ------------------------------------------------------------------ */
/* GPIO — LED on PD5                                                  */
/* ------------------------------------------------------------------ */

void gpio_led_init(void)
{
	DDRD |= (1 << PD5);
}

void gpio_led_toggle(void)
{
	PIND = (1 << PD5);
}

/* ------------------------------------------------------------------ */
/* platform_init — call once from main before sei()                   */
/* ------------------------------------------------------------------ */

void platform_init(void)
{
	/* nothing else needed; individual inits called by main */
}
