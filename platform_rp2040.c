#include <stdint.h>
#include "platform.h"

/* ================================================================
 * RP2040 platform implementation
 * USB CDC transport + SPI + Timer + EEPROM + GPIO
 * ================================================================ */

uint8_t           ring[RING_SIZE];
volatile uint16_t ring_head;
volatile uint16_t ring_tail;

/* ================================================================
 * USB CDC — TODO: implement via TinyUSB or custom
 * ================================================================ */

void uart_init(uint32_t baud)
{
	/* TODO: USB init sequence
	 * - Enable USB core clock
	 * - Setup USB PHY
	 * - Enumerate as CDC device
	 */
	(void)baud;
}

void uart_putc(uint8_t c)
{
	/* TODO: Write to CDC IN endpoint */
}

void uart_puts_P(const char *s)
{
	uint8_t c;
	while ((c = *s++) != 0)
		uart_putc(c);
}

uint8_t uart_getc_poll(void)
{
	/* TODO: Poll CDC OUT endpoint or UART fallback */
	return 0;
}

/* TODO: USB CDC interrupt handler or polling in main loop */

/* ================================================================
 * SPI — TODO: implement for RP2040
 * ================================================================ */

void spi_init(void)
{
	/* TODO: Configure SPI0
	 * - Enable SPI peripheral
	 * - Set pins: GPIO16 TX, GPIO17 RX, GPIO18 SCK, GPIO17 CS
	 * - Clock divider for ~125 kHz (SD init)
	 */
}

void spi_set_fast(void)
{
	/* TODO: Switch to ~8 MHz after SD init succeeds */
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

/* ================================================================
 * Timer — 500 ms for idle flush
 * ================================================================ */

static volatile uint8_t timer_pending = 0;

void timer_init(void)
{
	/* TODO: Configure Timer (Alarm 0)
	 * - Set 500 ms period
	 * - Enable interrupt
	 */
}

/* TODO: Timer interrupt handler */

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
	/* TODO: Reset timer or set new alarm */
}

/* ================================================================
 * EEPROM — TODO: use flash page
 * ================================================================ */

void eeprom_init(void)
{
	/* RP2040 has no EEPROM; use flash sector instead */
}

uint8_t eeprom_read_byte(uint16_t addr)
{
	/* TODO: Read from flash-based EEPROM */
	return 0;
}

void eeprom_write_byte(uint16_t addr, uint8_t val)
{
	/* TODO: Erase + write flash page */
	(void)addr;
	(void)val;
}

void eeprom_update_byte(uint16_t addr, uint8_t val)
{
	/* TODO: Check current value; only write if different */
	(void)addr;
	(void)val;
}

/* ================================================================
 * GPIO — LED on GPIO25 (onboard LED on Pico)
 * ================================================================ */

void gpio_led_init(void)
{
	/* TODO: Configure GPIO25 as output */
}

void gpio_led_toggle(void)
{
	/* TODO: Toggle GPIO25 */
}

/* ================================================================
 * Platform init
 * ================================================================ */

void platform_init(void)
{
	/* Placeholder */
}

/* ================================================================
 * SPI register inlines (for platform.h)
 * ================================================================ */

/* TODO: Implement in platform.h:
 * - spi_transfer(data) inline
 * - spi_cs_assert() inline
 * - spi_cs_release() inline
 */
