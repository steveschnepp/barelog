#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <avr/wdt.h>
#include "platform.h"
#include "config.h"
#include "spi.h"
#include "sd.h"
#include "fat32.h"
#include "log.h"

struct config cfg;

extern uint8_t __bss_end;
#define CANARY_ADDR  ((volatile uint16_t *)&__bss_end)
#define CANARY_VALUE 0xDEADU

#define ERR_SD_INIT   3
#define ERR_FAT_MOUNT 4
#define ERR_FAT_OPEN  5
#define ERR_STACK     6

static void blink_n(uint8_t n)
{
	uint8_t i;

	for (i = 0; i < n; i++) {
		gpio_led_toggle();
		delay_ms(200);
		gpio_led_toggle();
		delay_ms(200);
	}
}

__attribute__((noreturn))
void error_halt(uint8_t code)
{
	cli();
	for (;;) {
		blink_n(code);
		delay_ms(2000);
	}
}

int main(void)
{
	MCUSR = 0;
	wdt_disable();

	cli();

	PRR = (1 << PRTWI) | (1 << PRADC) | (1 << PRTIM2);

	gpio_led_init();
	*CANARY_ADDR = CANARY_VALUE;

	config_load(&cfg);

	DDRD  &= ~(1 << PD0);
	PORTD |=  (1 << PD0);
	delay_ms(1);
	if (!(PIND & (1 << PD0)) && !(cfg.flags & CFG_FL_IGNORE_RX_RST)) {
		config_defaults(&cfg);
		config_save(&cfg);
		blink_n(5);
	}

	platform_init();
	spi_init();

	if (sd_init() != SD_OK)
		error_halt(ERR_SD_INIT);

	if (fat32_mount() != 0)
		error_halt(ERR_FAT_MOUNT);

	if (fat32_open_log(&cfg) != 0)
		error_halt(ERR_FAT_OPEN);

	uart_init(cfg.baud);
	timer_init();

	sei();

	uart_putc('1');
	uart_putc('2');
	uart_putc('<');

	for (;;) {
		log_process();

		if (timer_flush_pending() && ring_available() < 512) {
			timer_clear_pending();
			log_flush();
			log_check_canary();

			set_sleep_mode(SLEEP_MODE_IDLE);
			sleep_enable();
			sleep_cpu();
			sleep_disable();
		}
	}
}
