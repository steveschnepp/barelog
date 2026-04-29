#include <stdint.h>
#include "platform.h"

/* ================================================================
 * ESP32-S3 platform implementation (single core)
 * USB CDC transport + SPI + Timer + Flash EEPROM + GPIO
 * 
 * ESP32-S3 (single core mode):
 *   - Xtensa 32-bit, 240 MHz (core 0 only, core 1 disabled)
 *   - USB Serial/JTAG (built-in) or USB OTG (external PHY)
 *   - SPI0 (flash), SPI1 (PSRAM), SPI2/SPI3 (general use)
 *   - TIMG0 for timers (core 0 only)
 *   - Flash 4-16 MB (typically 8 MB, via SPI NOR)
 *   - RAM 512 KB internal + PSRAM optional
 * 
 * Single-core simplifies: no multicore synchronization, simpler ISRs,
 * cleaner initialization. Core 1 is disabled at startup.
 * ================================================================ */

uint8_t           ring[RING_SIZE];
volatile uint16_t ring_head;
volatile uint16_t ring_tail;

/* ================================================================
 * USB CDC — TODO: implement using TinyUSB or custom CDC driver
 * ================================================================ */

void uart_init(uint32_t baud)
{
	/* TODO: Initialize USB
	 * Option 1: USB Serial/JTAG (USB_SERIAL_JTAG, simpler)
	 *   - Enable USB_SERIAL_JTAG clock
	 *   - Configure for CDC
	 *   - Internal DP/DM no external PHY needed
	 * 
	 * Option 2: USB OTG (USB_OTG, more flexible)
	 *   - Enable USB PHY
	 *   - Configure GPIO for D+/D-
	 *   - Setup OTG controller
	 *   - Use TinyUSB stack
	 * 
	 * For barelog, USB Serial/JTAG is simpler.
	 */
	(void)baud;
}

void uart_putc(uint8_t c)
{
	/* TODO: Write to CDC endpoint
	 * - Wait for TX ready (USB_SERIAL_JTAG.ep1_conf.wr_done)
	 * - Write to endpoint FIFO (USB_SERIAL_JTAG.ep1.w_fifo)
	 * - Mark done
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
	/* TODO: Poll CDC OUT endpoint
	 * - Check USB_SERIAL_JTAG.ep1_conf.rd_done
	 * - Read from FIFO (USB_SERIAL_JTAG.ep1.r_fifo)
	 * Or fallback to UART0 for command mode
	 */
	return 0;
}

/* TODO: USB Serial/JTAG interrupt handler
 * USB_SERIAL_JTAG_IRQHandler:
 *   - Handle RX on EP1 (OUT endpoint)
 *   - Feed ring[] from FIFO
 */

/* ================================================================
 * SPI — use SPI2 or SPI3 (SPI0/1 reserved for flash/PSRAM)
 * ================================================================ */

void spi_init(void)
{
	/* TODO: Configure SPI2
	 * - Enable SPI2 clock (SYSTEM.spi_clk_en)
	 * - Set pins: SCK=GPIO9, MOSI=GPIO11, MISO=GPIO13, CS=GPIO10
	 *   (adjust based on board)
	 * - Set frequency: 1 MHz for SD init (SPI2.clock register)
	 * - Set mode: SPI mode 0 (SPI2.ctrl register)
	 * - DMA disabled (polling mode for simplicity)
	 */
}

void spi_set_fast(void)
{
	/* TODO: Switch to 8 MHz after SD init
	 * - SPI2.clock register, set divider for 8 MHz
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
 * Timer — use TIMG0 (Timer Group 0)
 * ================================================================ */

static volatile uint8_t timer_pending = 0;

void timer_init(void)
{
	/* TODO: Configure TIMG0 Timer0 for 500 ms
	 * - TIMG0.T0CONFIG.enable = 0 (disable for config)
	 * - TIMG0.T0CONFIG.autoreload = 1
	 * - TIMG0.T0CONFIG.divider = 80 (APB 80 MHz / 80 = 1 MHz base)
	 * - TIMG0.T0ALARMLO = 500000 (500 ms at 1 MHz)
	 * - TIMG0.T0CONFIG.alarm_enable = 1
	 * - TIMG0.INT_ENA_TIMERS.T0_INT = 1 (enable interrupt)
	 * - Enable TIMG0_T0_LEVEL_IRQn in NVIC
	 * - TIMG0.T0CONFIG.enable = 1 (start)
	 */
}

/* TODO: TIMG0_T0_LEVEL_IRQHandler
 * if (TIMG0.INT_ST_TIMERS.T0_INT) {
 *	TIMG0.INT_CLR_TIMERS.T0_INT = 1;
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
	/* TODO: Reset TIMG0 Timer0 counter
	 * TIMG0.T0UPDATE.update = 1;
	 * TIMG0.T0LOADLO = 0;
	 * (or load from config if using reload)
	 */
}

/* ================================================================
 * Flash EEPROM — reserved partition or end of flash
 * ================================================================ */

void eeprom_init(void)
{
	/* Handled by eeprom_flash.h/c */
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
 * GPIO — LED on GPIO47 (typical ESP32-S3 DevKit)
 * ================================================================ */

void gpio_led_init(void)
{
	/* TODO: Configure GPIO47 as output
	 * - GPIO.enable_w1ts |= (1 << 47)
	 * - GPIO.out_w1tc = (1 << 47) to set low (LED on)
	 */
}

void gpio_led_toggle(void)
{
	/* TODO: Toggle GPIO47
	 * - GPIO.out ^= (1 << 47)
	 */
}

/* ================================================================
 * Platform init
 * ================================================================ */

void platform_init(void)
{
	/* TODO: Single-core setup
	 * - Disable core 1 (optional, ROM bootloader may not enable it)
	 *   Set PRO_CPU_RESET_REL in DPORT_APPCPU_CTRL_REG
	 * - Clock setup if needed
	 *   ESP32-S3 boots at 40 MHz from XTAL by default
	 *   May need to enable PLL for 240 MHz (done by ROM bootloader usually)
	 * - Initialize UART/USB before sei()
	 */
}

/* ================================================================
 * SPI register inlines (for platform.h)
 * ================================================================ */

/* TODO: Add to platform.h for ESP32-S3:
 * static inline uint8_t spi_transfer(uint8_t data)
 * {
 *	SPI2.mosi_dlen.val = 7;  // 8 bits
 *	SPI2.miso_dlen.val = 7;
 *	SPI2.data_buf[0] = data;
 *	SPI2.cmd.usr = 1;  // start transfer
 *	while (SPI2.cmd.usr);  // wait done
 *	return SPI2.data_buf[0];
 * }
 * 
 * static inline void spi_cs_assert(void)
 * {
 *	GPIO.out_w1tc = (1 << 10);  // GPIO10 low
 * }
 * 
 * static inline void spi_cs_release(void)
 * {
 *	GPIO.out_w1ts = (1 << 10);  // GPIO10 high
 * }
 */
