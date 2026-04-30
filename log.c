#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "log.h"
#include "uart.h"
#include "fat32.h"
#include "config.h"
#include "repl.h"

/* global config defined in main.c */
extern struct config cfg;

/*
 * Module state.
 * esc_seen:      number of consecutive escape characters seen so far.
 *                Reset to 0 on any non-escape byte.
 * flush_pending: set to 1 by Timer1 ISR. Cleared by main() before
 *                calling log_flush(). Declared volatile because it is
 *                written by an ISR and read in main-loop context.
 */
static struct {
	uint8_t          esc_seen;
	volatile uint8_t flush_pending;
} log_state;

/*
 * Stack canary: a known value placed just past the BSS segment by main()
 * before sei(). Checked after each idle flush to detect stack overflow.
 * __bss_end is provided by the linker; the address just past BSS is the
 * first address the stack can grow into from the top of SRAM.
 */
extern uint8_t __bss_end;
#define CANARY_ADDR  ((volatile uint16_t *)&__bss_end)
#define CANARY_VALUE 0xDEADU

/* error_halt is defined in main.c and never returns */
extern void error_halt(uint8_t code) __attribute__((noreturn));
#define ERR_STACK 6

/*
 * Scan buf[0..len-1] for the escape character.
 * Updates esc_seen across calls: state persists between span_a and span_b
 * so an escape sequence that straddles the ring wrap is detected correctly.
 * Resets esc_seen to 0 on any non-escape byte.
 * Returns 1 if esc_seen reaches cfg.esc_count, 0 otherwise.
 */
static uint8_t scan_escape(const uint8_t *buf, uint16_t len)
{
	uint16_t i;

	for (i = 0; i < len; i++) {
		if (buf[i] == cfg.esc_char) {
			log_state.esc_seen++;
			if (log_state.esc_seen >= cfg.esc_count)
				return 1;
		} else {
			log_state.esc_seen = 0;
		}
	}
	return 0;
}

void log_process(void)
{
	/*
	 * Process ring data in 512-byte chunks. Each iteration:
	 *   1. Snapshot ring_tail (one read; the ISR only advances head).
	 *   2. Compute up to two contiguous spans covering exactly 512 bytes,
	 *      handling the ring wrap boundary.
	 *   3. Scan both spans for the escape sequence before writing to SD.
	 *   4. Write the sector zero-copy directly from ring[].
	 *   5. Advance ring_tail by 512 bytes.
	 *   6. Reset TCNT1 to 0 to restart the 500 ms idle timer.
	 *      In CTC mode, writing TCNT1 does not affect OCF1A.
	 */
	while (uart_available() >= 512) {
		uint16_t       tail  = ring_tail;
		uint16_t       span1 = RING_SIZE - tail;
		const uint8_t *ptr_a = &ring[tail];
		uint16_t       len_a, len_b;
		const uint8_t *ptr_b;

		if (span1 >= 512) {
			/* data fits in one contiguous span, no wrap */
			len_a = 512;
			len_b = 0;
			ptr_b = NULL;
		} else {
			/* data wraps: span_a runs to end of ring, span_b from start */
			len_a = span1;
			len_b = 512 - span1;
			ptr_b = &ring[0];
		}

		/*
		 * Scan for escape before committing to SD. esc_seen state
		 * carries across the two span calls so a run split at the
		 * wrap boundary is detected correctly.
		 */
		if (scan_escape(ptr_a, len_a))
			repl_enter(); /* noreturn */
		if (ptr_b && scan_escape(ptr_b, len_b))
			repl_enter(); /* noreturn */

		/* ignore return: unrecoverable SD error, partial data preserved */
		fat32_append_sector(ptr_a, len_a, ptr_b, len_b);
		uart_consume(512);

		TCNT1 = 0; /* restart 500 ms idle timer */
	}
}

void log_flush(void)
{
	uint16_t avail = uart_available();

	if (avail > 0) {
		/*
		 * Pad the partial sector into ring[0..511].
		 *
		 * ring[0..511] is used as the scratch (pad) buffer.
		 * ring[tail..] holds the live data to copy.
		 *
		 * Ring safety: this function is called either from command
		 * mode (ISR off — ring fully safe) or from the idle path
		 * (ISR running, avail < 512). In the idle path, a collision
		 * occurs only if ring_head falls in [0..511] while we write
		 * there. This is possible when tail < 512 and head < 512.
		 * The risk is low (requires head and tail in the same lower
		 * half with < 512 bytes between them during the ~100 µs
		 * copy loop) but is not zero. A future revision should check
		 * that tail >= 512 before proceeding in the ISR-live path.
		 *
		 * Copy order: ptr_b (ring[0..len_b-1]) is copied into
		 * pad[len_a..] before ptr_a (ring[tail..]) into pad[0..len_a-1].
		 * This avoids clobbering ptr_b's source in the wrap case
		 * where ptr_b == ring[0] and pad == ring[0].
		 */
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

		/* copy ptr_b first to avoid overlap clobber in wrap case */
		if (ptr_b) {
			uint16_t k;
			for (k = 0; k < len_b; k++)
				pad[len_a + k] = ptr_b[k];
		}
		for (j = 0; j < len_a; j++)
			pad[j] = ptr_a[j];

		/* zero-pad remainder of sector */
		for (j = len_a + len_b; j < 512; j++)
			pad[j] = 0x00;

		/* ignore return: see log_process comment */
		fat32_append_sector(pad, 512, NULL, 0);
		uart_consume(avail);
	}

	/* update directory entry file_size — ignore return */
	fat32_flush();

	/*
	 * Write bytes_written recovery record to EEPROM.
	 * Uses eeprom_update_byte (not eeprom_write_byte) to skip the
	 * write if the value is unchanged, reducing flash wear.
	 * Stored as four bytes (two 16-bit halves, little-endian) so each
	 * half can be updated independently in a single byte write.
	 */
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
	if (*CANARY_ADDR != CANARY_VALUE)
		error_halt(ERR_STACK);
}

uint8_t log_flush_pending(void)
{
	return log_state.flush_pending;
}

void log_clear_flush_pending(void)
{
	log_state.flush_pending = 0;
}
