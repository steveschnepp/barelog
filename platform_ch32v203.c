#include <stdint.h>
#include "platform.h"

/* ================================================================
 * CH32V203C8T6 platform implementation
 * USB CDC transport + SPI + Timer + EEPROM + GPIO
 * ================================================================ */

uint8_t           ring[RING_SIZE];
volatile uint16_t ring_head;
volatile uint16_t ring_tail;

/* ================================================================
 * USB CDC — TODO: implement descriptors, enumeration, endpoints
 * ================================================================ */

void uart_init(uint32_t baud)
{
	/* TODO: USB init sequence
	 * - Enable USB clock and peripheral
	 * - Configure USB pull-up on DP
	 * - Setup endpoints (EP0 control, EP1 IN, EP2 OUT)
	 * - Enable USB interrupt (low-speed or full-speed)
	 */
	(void)baud; /* USB speed ignored */
}

void uart_putc(uint8_t c)
{
	/* TODO: Write to CDC IN endpoint (EP1)
	 * - Wait for endpoint ready
	 * - Write byte to endpoint buffer
	 * - Set endpoint data ready
	 */
}

void uart_puts_P(const char *s)
{
	uint8_t c;
	while ((c = *s++) != 0)
		uart_putc(c);
}

uint8_t uart_getc_poll(void)
{
	/* TODO: Poll CDC OUT endpoint (EP2) or UDR0 fallback
	 * - Check endpoint has data
	 * - Read and return byte
	 */
	return 0;
}

/* TODO: USB CDC OUT endpoint ISR
 * Feeds ring[] same as UART RX ISR
 */

/* ================================================================
 * SPI — TODO: implement for CH32V203
 * ================================================================ */

void spi_init(void)
{
	/* TODO: Configure SPI1
	 * - Enable SPI peripheral
	 * - Set pins: PA5 SCK, PA6 MISO, PA7 MOSI, PA4 CS
	 * - Clock fosc/128 for SD init
	 */
}

void spi_set_fast(void)
{
	/* TODO: Switch to fosc/2 after SD init succeeds */
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
	/* TODO: Configure Timer1 (or TIM2)
	 * - 144 MHz / prescaler = base frequency
	 * - 500 ms period
	 * - Enable update interrupt
	 */
}

/* TODO: Timer interrupt handler
 * Sets timer_pending = 1
 */

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
	/* TODO: Reset timer counter to 0 */
}

/* ================================================================
 * EEPROM — TODO: implement using Flash pages
 * ================================================================ */

void eeprom_init(void)
{
	/* CH32V203 has no EEPROM; use flash sector instead */
}

uint8_t eeprom_read_byte(uint16_t addr)
{
	/* TODO: Read from flash-based EEPROM at addr */
	return 0;
}

void eeprom_write_byte(uint16_t addr, uint8_t val)
{
	/* TODO: Erase + write flash page containing addr */
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
 * GPIO — LED on PC13 (typical for WeAct BluePill+)
 * ================================================================ */

void gpio_led_init(void)
{
	/* TODO: Configure PC13 as output */
}

void gpio_led_toggle(void)
{
	/* TODO: Toggle PC13 */
}

/* ================================================================
 * Platform init
 * ================================================================ */

void platform_init(void)
{
	/* Placeholder; individual inits called from main */
}

/* ================================================================
 * SPI register inlines (for platform.h)
 * ================================================================ */

/* TODO: Implement in platform.h:
 * - spi_transfer(data) inline
 * - spi_cs_assert() inline
 * - spi_cs_release() inline
 */
