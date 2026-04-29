#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include "config.h"
#include "uart.h"
#include "spi.h"
#include "sd.h"
#include "fat32.h"
#include "log.h"
#include <avr/wdt.h>
#include "util.h"

/*
 * Global config instance. The only global besides ring[], ring_head,
 * ring_tail (declared in uart.h). Accessed by log.c via extern.
 */
struct config cfg;

/*
 * Stack canary. Placed just past BSS by main() before sei().
 * Checked by log_check_canary() after each idle flush.
 *
 * NOTE: CANARY_ADDR and CANARY_VALUE are also defined in log.c.
 * Both definitions must agree. A future refactor should move them
 * to a shared internal header.
 */
extern uint8_t __bss_end;
#define CANARY_ADDR  ((volatile uint16_t *)&__bss_end)
#define CANARY_VALUE 0xDEADU

/*
 * Error codes for error_halt(). The value is the number of LED blinks
 * per cycle, making errors distinguishable without a debugger.
 */
#define ERR_SD_INIT   3  /* SD card init failed */
#define ERR_FAT_MOUNT 4  /* FAT32 mount failed */
#define ERR_FAT_OPEN  5  /* log file open/create failed */
#define ERR_STACK     6  /* stack canary clobbered */

/*
 * Blink the STAT LED n times with 200 ms on/off periods.
 * Only called before sei() (factory reset) or after cli() (error_halt),
 * so the read-modify-write on PORTD is safe from ISR interference.
 */
static void blink_n(uint8_t n)
{
	uint8_t i;

	for (i = 0; i < n; i++) {
		PORTD |=  (1 << PD5);
		delay_ms(200);
		PORTD &= ~(1 << PD5);
		delay_ms(200);
	}
}

/*
 * Halt with a repeating blink pattern indicating the error code.
 * Disables interrupts so the UART ISR cannot interfere with the LED.
 * Never returns.
 */
__attribute__((noreturn))
void error_halt(uint8_t code)
{
	cli();
	for (;;) {
		blink_n(code);
		delay_ms(2000); /* pause between blink groups */
	}
}

int main(void)
{
	/* ---- 0. disable watchdog ------------------------------------
	 * Optiboot does not clear MCUSR before jumping to the app.
	 * If the WDE bit is set (left over from a watchdog reset via
	 * cmd_reset), the watchdog is still running at WDTO_15MS and
	 * will reset the MCU again before init completes — boot loop.
	 * Clear MCUSR first (required by the datasheet before wdt_disable
	 * can take effect), then disable the watchdog.
	 */
	MCUSR = 0;
	wdt_disable();

	/* ---- 1. disable interrupts ----------------------------------- */
	cli();

	/*
	 * ---- 2. disable unused peripherals --------------------------
	 * PRR bits: PRTWI (TWI/I2C), PRADC (ADC), PRTIM2 (Timer2).
	 * SPI (PRSPI) and Timer0/1 (PRTIM0, PRTIM1) are left enabled.
	 * UART (PRUSART0) is left enabled for uart_init() below.
	 * Disabling clocks saves ~1 mA and prevents spurious interrupts.
	 */
	PRR = (1 << PRTWI) | (1 << PRADC) | (1 << PRTIM2);

	/*
	 * ---- 3. configure STAT LED ----------------------------------
	 * PD5 is the STAT1 LED. Output mode; start low (off).
	 * The UART ISR toggles it via PIND (atomic toggle, no RMW).
	 */
	DDRD |= (1 << PD5);

	/* ---- 4. place stack canary ----------------------------------- */
	*CANARY_ADDR = CANARY_VALUE;

	/* ---- 5. load config from EEPROM ----------------------------- */
	config_load(&cfg);

	/*
	 * ---- 6. factory reset check ---------------------------------
	 * If PD0 (RX) is held low at boot and CFG_FL_IGNORE_RX_RST is
	 * not set, reset config to defaults and save. Blink 5 times to
	 * confirm the reset to the user.
	 * PD0 is configured as input with pull-up, then sampled after 1 ms
	 * to allow the line to settle.
	 */
	DDRD  &= ~(1 << PD0);
	PORTD |=  (1 << PD0);
	delay_ms(1);
	if (!(PIND & (1 << PD0)) && !(cfg.flags & CFG_FL_IGNORE_RX_RST)) {
		config_defaults(&cfg);
		config_save(&cfg);
		blink_n(5);
	}

	/* ---- 7. init SPI at fosc/128 (slow, for SD init) ------------ */
	spi_init();

	/*
	 * ---- 8. init SD card ----------------------------------------
	 * Runs the full SD SPI init sequence. Switches SPI to fosc/2
	 * on success. Calls error_halt on failure (3 blinks).
	 */
	if (sd_init() != SD_OK)
		error_halt(ERR_SD_INIT);

	/*
	 * ---- 9. mount FAT32 -----------------------------------------
	 * Reads MBR and VBR into ring[0..511]. Safe: UART ISR not yet
	 * enabled, ring is unused scratch.
	 */
	if (fat32_mount() != 0)
		error_halt(ERR_FAT_MOUNT);

	/*
	 * ---- 10. open or resume log file ----------------------------
	 * Creates LOGnnnnn.TXT, pre-allocates, and zero-fills it.
	 * Or resumes a previous session using the EEPROM recovery offset.
	 * Uses ring[0..511]. Still safe: ISR not yet enabled.
	 */
	if (fat32_open_log(&cfg) != 0)
		error_halt(ERR_FAT_OPEN);

	/*
	 * ---- 11. init UART ------------------------------------------
	 * Enables RX, TX, and RXCIE. Does not call sei().
	 * Bytes arriving from this point are queued in ring[] by the ISR
	 * once sei() is called below.
	 */
	uart_init(cfg.baud);

	/*
	 * ---- 12. init Timer1 CTC, 500 ms period --------------------
	 * CTC mode: TCCR1B WGM12=1. Counter resets to 0 on match with OCR1A.
	 * Prescaler CS12=1, CS10=1: fosc/1024 = 15625 Hz at 16 MHz.
	 * OCR1A = 15625 / 2 - 1 = 7812 → match every 500 ms.
	 * OCIE1A enables the compare-match interrupt handled in log.c.
	 */
	TCCR1A = 0;
	TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10);
	OCR1A  = 7812;
	TIMSK1 = (1 << OCIE1A);
	TCNT1  = 0;

	/* ---- 13. enable global interrupts --------------------------- */
	sei();

	/*
	 * ---- 14. signal ready ---------------------------------------
	 * Boot sequence matches the original SparkFun OpenLog protocol:
	 *   '1' — UART initialised and alive
	 *   '2' — SD card and FAT32 mount succeeded
	 *   '<' — log file open, entering logging mode
	 * Hosts that parse the boot banner expect all three characters.
	 */
	uart_putc('1');
	uart_putc('2');
	uart_putc('<');

	/*
	 * ---- 15. main loop ------------------------------------------
	 * log_process() drains the ring to SD in 512-byte zero-copy writes.
	 * When the idle timer fires (flush_pending set) and fewer than 512
	 * bytes remain, we flush the partial sector, check the canary, then
	 * sleep in IDLE mode. IDLE keeps the UART clock running so the ISR
	 * can still receive bytes and will wake the CPU immediately.
	 *
	 * Sleep safety: if an ISR fires between the flush_pending check and
	 * sleep_cpu(), the sleep wakes on the very next interrupt. No bytes
	 * are lost.
	 */
	for (;;) {
		log_process();

		if (log_flush_pending() && uart_available() < 512) {
			log_clear_flush_pending();
			log_flush();
			log_check_canary();

			set_sleep_mode(SLEEP_MODE_IDLE);
			sleep_enable();
			sleep_cpu();   /* wake on any interrupt (UART RX or Timer1) */
			sleep_disable();
		}
	}
}
