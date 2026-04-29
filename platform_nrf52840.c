#include <stdint.h>
#include "platform.h"

/* ================================================================
 * nRF52840 platform implementation
 * USB CDC transport + SPI + Timer + Flash EEPROM + GPIO
 * 
 * nRF52840:
 *   - Cortex-M4, 64 MHz
 *   - USB 2.0 device controller
 *   - SPIM (SPI Master) instances 0-3
 *   - TIMER0-4 for timing
 *   - Flash 1 MB (0x00000000 - 0x000FFFFF)
 *   - RAM 256 KB (0x20000000 - 0x2003FFFF)
 * ================================================================ */

uint8_t           ring[RING_SIZE];
volatile uint16_t ring_head;
volatile uint16_t ring_tail;

/* ================================================================
 * USB CDC — TODO: implement using nRF52 USB driver
 * ================================================================ */

void uart_init(uint32_t baud)
{
	/* TODO: Initialize USB 2.0 device controller
	 * - Enable USB clock (POWER.USBREGSTATUS)
	 * - Configure pull-up on DP (GPIO P0.06 typical)
	 * - Setup endpoints (EP0 control, EP1/2 IN/OUT for CDC)
	 * - Initialize CDC state machine
	 * - Enable USB interrupt (NVIC)
	 */
	(void)baud;
}

void uart_putc(uint8_t c)
{
	/* TODO: Write to CDC IN endpoint
	 * - Wait for endpoint ready (USBD_EPSTATUS.EPIN[1])
	 * - Write to endpoint buffer (USBD.EPIN[1].PTR, EPIN[1].MAXCNT)
	 * - Trigger transfer
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
	/* TODO: Poll CDC OUT endpoint or UARTE fallback
	 * - Check USBD_EPSTATUS.EPOUT[2] for data ready
	 * - Read from endpoint buffer
	 * Or fallback to UARTE for command mode if USB unavailable
	 */
	return 0;
}

/* TODO: USB CDC endpoint ISRs
 * USBD_IRQHandler:
 *   - EPDATA event: feed ring[] from EPOUT[2]
 *   - Keep track of endpoint state
 */

/* ================================================================
 * SPI — use SPIM2 or SPIM3 (0/1 reserved for flash/BLE)
 * ================================================================ */

void spi_init(void)
{
	/* TODO: Configure SPIM2
	 * - Enable peripheral (SPIM2.ENABLE)
	 * - Set pins: SCK=GPIO_XX, MOSI=GPIO_XX, MISO=GPIO_XX, CS=GPIO_XX
	 *   (Common: SCK=P0.19, MOSI=P0.17, MISO=P0.20, CS=P0.21)
	 * - Set frequency: 125 kHz for SD init (SPIM2.FREQUENCY)
	 * - Set mode: SPI mode 0 (SPIM2.CONFIG)
	 * - Set ORC (over-read character) to 0xFF
	 */
}

void spi_set_fast(void)
{
	/* TODO: Switch to 8 MHz after SD init
	 * - SPIM2.FREQUENCY = SPIM_FREQUENCY_FREQUENCY_M8
	 */
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
 * Timer — use TIMER1 (TIMER0 reserved for something else often)
 * ================================================================ */

static volatile uint8_t timer_pending = 0;

void timer_init(void)
{
	/* TODO: Configure TIMER1 for 500 ms period
	 * - TIMER1.MODE = TIMER (not counter)
	 * - TIMER1.BITMODE = 32-bit
	 * - TIMER1.PRESCALER = 4 (16 MHz / 2^4 = 1 MHz)
	 * - TIMER1.CC[0] = 500000 (500 ms at 1 MHz)
	 * - TIMER1.SHORTS = COMPARE0_CLEAR (auto-reset)
	 * - TIMER1.INTENSET = COMPARE0 (enable interrupt)
	 * - Enable TIMER1_IRQn in NVIC
	 */
}

/* TODO: TIMER1_IRQHandler
 * if (NRF_TIMER1->EVENTS_COMPARE[0]) {
 *	NRF_TIMER1->EVENTS_COMPARE[0] = 0;
 *	timer_pending = 1;
 * }
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
	/* TODO: Clear TIMER1 counter to restart 500 ms window
	 * NRF_TIMER1->TASKS_CLEAR = 1;
	 */
}

/* ================================================================
 * Flash EEPROM — last 4 KB of flash
 * ================================================================ */

void eeprom_init(void)
{
	/* Included via eeprom_flash.h, implement in separate file */
}

uint8_t eeprom_read_byte(uint16_t addr)
{
	/* Handled by eeprom_flash.c */
	return 0;
}

void eeprom_write_byte(uint16_t addr, uint8_t val)
{
	/* Handled by eeprom_flash.c */
	(void)addr;
	(void)val;
}

void eeprom_update_byte(uint16_t addr, uint8_t val)
{
	/* Handled by eeprom_flash.c */
	(void)addr;
	(void)val;
}

/* ================================================================
 * GPIO — LED on P0.13 (typical Feather nRF52840)
 * ================================================================ */

void gpio_led_init(void)
{
	/* TODO: Configure P0.13 as output
	 * - NRF_GPIO->PIN_CNF[13] = GPIO_PIN_CNF_DIR_Output
	 * - NRF_GPIO->OUTSET = (1 << 13) to set high (LED off, active low)
	 */
}

void gpio_led_toggle(void)
{
	/* TODO: Toggle P0.13
	 * - NRF_GPIO->OUT ^= (1 << 13)
	 */
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

/* TODO: Add to platform.h for nRF52840:
 * static inline uint8_t spi_transfer(uint8_t data)
 * {
 *	NRF_SPIM2->TXD.PTR = (uint32_t)&data;
 *	NRF_SPIM2->TXD.MAXCNT = 1;
 *	NRF_SPIM2->RXD.PTR = (uint32_t)&data;
 *	NRF_SPIM2->RXD.MAXCNT = 1;
 *	NRF_SPIM2->TASKS_START = 1;
 *	while (!NRF_SPIM2->EVENTS_END);
 *	NRF_SPIM2->EVENTS_END = 0;
 *	return data;
 * }
 * 
 * static inline void spi_cs_assert(void)
 * {
 *	NRF_GPIO->OUTCLR = (1 << 21);  // P0.21 low
 * }
 * 
 * static inline void spi_cs_release(void)
 * {
 *	NRF_GPIO->OUTSET = (1 << 21);  // P0.21 high
 * }
 */
