#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "log.h"
#include "platform.h"
#include "fat32.h"
#include "config.h"
#include "repl.h"

extern struct config cfg;

extern void error_halt(uint8_t code) __attribute__((noreturn));
#define ERR_STACK 6

static uint8_t scan_escape(const uint8_t *buf, uint16_t len)
{
	static uint8_t esc_seen = 0;
	uint16_t i;

	for (i = 0; i < len; i++) {
		if (buf[i] == cfg.esc_char) {
			esc_seen++;
			if (esc_seen >= cfg.esc_count)
				return 1;
		} else {
			esc_seen = 0;
		}
	}
	return 0;
}

void log_process(void)
{
	while (ring_available() >= 512) {
		uint16_t       tail  = ring_tail;
		uint16_t       span1 = RING_SIZE - tail;
		const uint8_t *ptr_a = &ring[tail];
		uint16_t       len_a, len_b;
		const uint8_t *ptr_b;

		if (span1 >= 512) {
			len_a = 512;
			len_b = 0;
			ptr_b = NULL;
		} else {
			len_a = span1;
			len_b = 512 - span1;
			ptr_b = &ring[0];
		}

		if (scan_escape(ptr_a, len_a))
			repl_enter();
		if (ptr_b && scan_escape(ptr_b, len_b))
			repl_enter();

		fat32_append_sector(ptr_a, len_a, ptr_b, len_b);
		ring_consume(512);

		timer_restart();
	}
}

void log_flush(void)
{
	uint16_t avail = ring_available();

	if (avail > 0) {
		uint16_t       tail  = ring_tail;
		uint16_t       span1 = RING_SIZE - tail;
		const uint8_t *ptr_a = &ring[tail];
		uint16_t       len_a, len_b;
		const uint8_t *ptr_b;
		uint8_t       *pad = ring;
		uint16_t       j;

		if (span1 >= avail) {
			len_a = avail;
			len_b = 0;
			ptr_b = NULL;
		} else {
			len_a = span1;
			len_b = avail - span1;
			ptr_b = &ring[0];
		}

		if (ptr_b) {
			uint16_t k;
			for (k = 0; k < len_b; k++)
				pad[len_a + k] = ptr_b[k];
		}
		for (j = 0; j < len_a; j++)
			pad[j] = ptr_a[j];

		for (j = len_a + len_b; j < 512; j++)
			pad[j] = 0x00;

		fat32_append_sector(pad, 512, NULL, 0);
		ring_consume(avail);
	}

	fat32_flush();

	{
		uint32_t bw = fat32_bytes_written();
		uint16_t lo = (uint16_t)(bw & 0xFFFF);
		uint16_t hi = (uint16_t)(bw >> 16);
		uint8_t *p;

		p = (uint8_t *)EEPROM_OFF_REC_LO;
		eeprom_update_byte(p,     (uint8_t)(lo & 0xFF));
		eeprom_update_byte(p + 1, (uint8_t)(lo >> 8));

		p = (uint8_t *)EEPROM_OFF_REC_HI;
		eeprom_update_byte(p,     (uint8_t)(hi & 0xFF));
		eeprom_update_byte(p + 1, (uint8_t)(hi >> 8));
	}
}

void log_check_canary(void)
{
	extern uint8_t __bss_end;
	volatile uint16_t *canary = (volatile uint16_t *)&__bss_end;
	if (*canary != 0xDEADU)
		error_halt(ERR_STACK);
}
