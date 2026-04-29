#include <stdint.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "repl.h"
#include "uart.h"
#include "fat32.h"
#include "sd.h"
#include "log.h"

/*
 * Command mode memory layout:
 *
 *   ring[0..511]    SD sector scratch.
 *                   fat32 and sd functions already use ring[] as their
 *                   sector buffer. No change needed there.
 *
 *   ring[512..1022] Line input buffer (LINE_BUF). 511 usable bytes + NUL.
 *                   Longest command is "trim fuller" (11 chars).
 *                   The extra space is intentional: we accept long lines
 *                   silently truncated rather than crashing.
 *
 *   ring[1023]      Implicit NUL sentinel. Never written by line reader
 *                   because LINE_BUF_MAX caps at 511 (index 1022).
 *
 * UART receive in command mode:
 *   RXCIE0 is cleared on entry so the ring ISR never fires.
 *   The hardware USART receiver stays active — bytes accumulate in UDR0.
 *   repl_getc() polls RXC0 and reads UDR0 directly, bypassing the ring
 *   queue entirely. This is safe: UDR0 is a double-buffered hardware
 *   register; we will not miss a byte as long as we read before the next
 *   one arrives (true at human typing speeds).
 *
 * Exit:
 *   Only the reset command exits command mode. It enables the watchdog
 *   with a 15 ms timeout and spins. The watchdog fires, reboots the MCU,
 *   and uart_init() in main() re-enables RXCIE0.
 */

/* ring[0..511]: SD scratch — matches what fat32/sd already expect */
#define SD_BUF       (ring)

/* ring[512..1022]: line input buffer */
#define LINE_BUF     (ring + 512)
#define LINE_BUF_MAX 511u

/* ------------------------------------------------------------------ */
/* UART receive — direct hardware poll, ISR disabled                  */
/*                                                                     */
/* RXC0 is set by hardware when UDR0 holds an unread byte.            */
/* Reading UDR0 clears RXC0 and arms the receiver for the next byte.  */
/* ------------------------------------------------------------------ */

static uint8_t repl_getc(void)
{
	/* spin until the USART receiver has a byte ready */
	while (!(UCSR0A & (1 << RXC0)))
		;
	return UDR0;
}

/* ------------------------------------------------------------------ */
/* output helpers                                                      */
/* ------------------------------------------------------------------ */

static void repl_putc(uint8_t c)
{
	uart_putc(c);
}

static void repl_puts_P(const char *s)
{
	uart_puts_P(s);
}

static void repl_crlf(void)
{
	uart_putc('\r');
	uart_putc('\n');
}

/*
 * Print v in decimal. Digits are collected in reverse into a 10-byte
 * stack buffer (max uint32 is 4294967295, 10 digits), then emitted
 * MSB-first. No heap, no stdio.
 */
static void repl_putu32(uint32_t v)
{
	uint8_t buf[10];
	uint8_t i = 0;

	if (v == 0) {
		repl_putc('0');
		return;
	}
	while (v > 0) {
		buf[i++] = (uint8_t)(v % 10) + '0';
		v /= 10;
	}
	while (i > 0)
		repl_putc(buf[--i]);
}

/* ------------------------------------------------------------------ */
/* line reader                                                         */
/*                                                                     */
/* Reads bytes one at a time from the USART hardware via repl_getc.  */
/* Stores into LINE_BUF. Echoes each printable character back.        */
/* CR or LF terminates the line and NUL-terminates the buffer.        */
/* Backspace (0x08) and DEL (0x7F) erase the last character with the  */
/* VT100 sequence BS SP BS, keeping the terminal in sync.             */
/* Characters beyond LINE_BUF_MAX are silently dropped; the buffer    */
/* is never overrun.                                                   */
/* ------------------------------------------------------------------ */

static void repl_read_line(void)
{
	uint16_t len = 0;
	uint8_t  c;

	for (;;) {
		c = repl_getc();

		if (c == '\r' || c == '\n') {
			/* end of line: terminate buffer and echo newline */
			LINE_BUF[len] = 0;
			repl_crlf();
			return;
		}

		if (c == 0x7F || c == 0x08) {
			/* backspace: erase last char on terminal and in buffer */
			if (len > 0) {
				len--;
				repl_putc(0x08); /* cursor left */
				repl_putc(' ');  /* overwrite char with space */
				repl_putc(0x08); /* cursor left again */
			}
			continue;
		}

		if (len < LINE_BUF_MAX) {
			/* printable byte: store and echo */
			LINE_BUF[len++] = c;
			repl_putc(c);
		}
		/* else: silently drop — buffer full */
	}
}

/* ------------------------------------------------------------------ */
/* line_eq: compare LINE_BUF against a compile-time string literal    */
/*                                                                     */
/* Returns 1 if LINE_BUF content is identical to b, 0 otherwise.     */
/* Stops at NUL terminator in b; checks that LINE_BUF also ends there */
/* so "disk" does not match "diskfoo".                                */
/* ------------------------------------------------------------------ */

static uint8_t line_eq(const char *b)
{
	const uint8_t *a = LINE_BUF;

	while (*b) {
		if (*a != (uint8_t)*b)
			return 0;
		a++;
		b++;
	}
	/* both pointers must land on NUL simultaneously */
	return *a == 0;
}

/* ------------------------------------------------------------------ */
/* command handlers                                                    */
/* ------------------------------------------------------------------ */

/* Print the list of supported commands. */
static void cmd_help(void)
{
	repl_puts_P(PSTR("?\r\n"
	                 "disk\r\n"
	                 "init\r\n"
	                 "sync\r\n"
	                 "reset\r\n"
	                 "trim free\r\n"
	                 "trim full\r\n"
	                 "trim fuller\r\n"));
}

/*
 * Print the mounted FAT32 volume layout.
 * All values are in 512-byte sectors unless noted.
 * size_mb is derived as tot_sec / 2048 (2048 sectors = 1 MiB).
 */
static void cmd_disk(void)
{
	uint32_t part_lba = fat32_vol_part_lba();  /* partition start on card */
	uint32_t fat_lba  = fat32_vol_fat_lba();   /* first FAT sector */
	uint32_t fat_sz32 = fat32_vol_fat_sz32();  /* sectors per FAT copy */
	uint8_t  num_fats = fat32_vol_num_fats();  /* number of FAT copies (2) */
	uint32_t data_lba = fat32_vol_data_lba();  /* first data cluster sector */
	uint8_t  spc      = fat32_vol_sec_per_clus(); /* sectors per cluster */
	uint32_t tot_sec  = fat32_vol_tot_sec();   /* total sectors in partition */

	repl_puts_P(PSTR("part_lba=")); repl_putu32(part_lba); repl_crlf();
	repl_puts_P(PSTR("fat_lba="));  repl_putu32(fat_lba);  repl_crlf();
	repl_puts_P(PSTR("fat_sz32=")); repl_putu32(fat_sz32); repl_crlf();
	repl_puts_P(PSTR("num_fats=")); repl_putu32(num_fats); repl_crlf();
	repl_puts_P(PSTR("data_lba=")); repl_putu32(data_lba); repl_crlf();
	repl_puts_P(PSTR("sec/clus=")); repl_putu32(spc);      repl_crlf();
	repl_puts_P(PSTR("tot_sec="));  repl_putu32(tot_sec);  repl_crlf();
	repl_puts_P(PSTR("size_mb="));  repl_putu32(tot_sec / 2048UL); repl_crlf();
}

/*
 * Re-run SD init and FAT32 mount.
 * Useful after a card swap without power cycle.
 * Does not re-open the log file; use reset for that.
 */
static void cmd_init(void)
{
	if (sd_init() != SD_OK || fat32_mount() != 0)
		repl_puts_P(PSTR("!\r\n"));
	else
		repl_puts_P(PSTR("ok\r\n"));
}

/*
 * Flush the in-memory log state to SD and update the directory entry.
 * Equivalent to what the idle timer triggers automatically in log mode.
 * Safe to call here: ISR is off, ring[0..511] is free SD scratch.
 */
static void cmd_sync(void)
{
	log_flush();
	repl_puts_P(PSTR("ok\r\n"));
}

/*
 * Reboot the MCU via watchdog.
 * The 15 ms timeout is the shortest available on ATmega328.
 * After reboot, main() runs uart_init() which re-enables RXCIE0,
 * and fat32_open_log() creates or resumes the next log file.
 */
static void cmd_reset(void)
{
	repl_puts_P(PSTR("reset\r\n"));
	wdt_enable(WDTO_15MS);
	for (;;)   /* watchdog fires within 15 ms */
		;
}

/*
 * Erase all unallocated clusters on the FAT32 volume.
 * Reads each FAT sector into ring[0..511], finds clusters with entry 0,
 * coalesces contiguous free runs, and issues one sd_erase() per run.
 * The SD card's internal erase unit may be larger than one cluster;
 * coalescing minimises the number of erase commands sent.
 * Does not modify the FAT or directory. The volume remains mountable.
 */
static void cmd_trim_free(void)
{
	int8_t ret;

	repl_puts_P(PSTR("trimming free...\r\n"));
	ret = fat32_trim_free();
	repl_puts_P(ret == 0 ? PSTR("ok\r\n") : PSTR("!\r\n"));
}

/*
 * Erase all data sectors on the FAT32 volume (data_lba to end).
 * MBR, BPB, and FAT regions are preserved. The volume structure
 * survives but all file content is erased. Useful before returning
 * a card to a pool without destroying the filesystem.
 */
static void cmd_trim_full(void)
{
	repl_puts_P(PSTR("trimming data...\r\n"));
	fat32_trim_full();
	repl_puts_P(PSTR("ok\r\n"));
}

/*
 * Erase the entire card from LBA 0 to end of partition.
 * MBR, BPB, FAT, and all data are erased. The card must be
 * reformatted before use. Use when decommissioning a card.
 */
static void cmd_trim_fuller(void)
{
	repl_puts_P(PSTR("trimming card...\r\n"));
	fat32_trim_fuller();
	repl_puts_P(PSTR("ok\r\n"));
}

/* ------------------------------------------------------------------ */
/* dispatch                                                            */
/*                                                                     */
/* Match LINE_BUF against each known command string. line_eq does an  */
/* exact full-string match so no prefix ambiguity is possible.        */
/* "trim full" is checked before "trim fuller" is irrelevant here     */
/* because line_eq requires the full string to match, but the order   */
/* is kept most-specific-first as a convention.                       */
/* ------------------------------------------------------------------ */

static void repl_dispatch(void)
{
	if      (line_eq("?"))           cmd_help();
	else if (line_eq("disk"))        cmd_disk();
	else if (line_eq("init"))        cmd_init();
	else if (line_eq("sync"))        cmd_sync();
	else if (line_eq("reset"))       cmd_reset();
	else if (line_eq("trim free"))   cmd_trim_free();
	else if (line_eq("trim fuller")) cmd_trim_fuller(); /* before "trim full" */
	else if (line_eq("trim full"))   cmd_trim_full();
	else                             repl_puts_P(PSTR("!\r\n"));
}

/* ------------------------------------------------------------------ */
/* entry point                                                         */
/* ------------------------------------------------------------------ */

void repl_enter(void)
{
	/*
	 * Kill the RX interrupt. From this point the ISR never fires and
	 * ring[0..1023] is stable scratch. The USART hardware receiver
	 * remains active; repl_getc() reads UDR0 directly via polling.
	 */
	UCSR0B &= ~(1 << RXCIE0);

	/*
	 * If a byte arrived between the escape detection and this point,
	 * it is sitting in UDR0. Discard it: it is part of the escape
	 * sequence or noise, not a command.
	 */
	if (UCSR0A & (1 << RXC0))
		(void)UDR0;

	/* commit any buffered log data to SD before going interactive */
	log_flush();

	repl_puts_P(PSTR("\r\n>"));

	for (;;) {
		repl_read_line();

		if (LINE_BUF[0] == 0) {
			/* empty line: re-emit prompt, do nothing */
			repl_putc('>');
			continue;
		}

		repl_dispatch();
		repl_putc('>');
	}
}
