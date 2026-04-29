#include <stdint.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "repl.h"
#include "platform.h"
#include "fat32.h"
#include "sd.h"
#include "log.h"

#define SD_BUF       (ring)
#define LINE_BUF     (ring + 512)
#define LINE_BUF_MAX 511u

static uint8_t repl_getc(void)
{
	return uart_getc_poll();
}

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

static void repl_read_line(void)
{
	uint16_t len = 0;
	uint8_t  c;

	for (;;) {
		c = repl_getc();

		if (c == '\r' || c == '\n') {
			LINE_BUF[len] = 0;
			repl_crlf();
			return;
		}

		if (c == 0x7F || c == 0x08) {
			if (len > 0) {
				len--;
				repl_putc(0x08);
				repl_putc(' ');
				repl_putc(0x08);
			}
			continue;
		}

		if (len < LINE_BUF_MAX) {
			LINE_BUF[len++] = c;
			repl_putc(c);
		}
	}
}

static uint8_t line_eq(const char *b)
{
	const uint8_t *a = LINE_BUF;

	while (*b) {
		if (*a != (uint8_t)*b)
			return 0;
		a++;
		b++;
	}
	return *a == 0;
}

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

static void cmd_disk(void)
{
	uint32_t part_lba = fat32_vol_part_lba();
	uint32_t fat_lba  = fat32_vol_fat_lba();
	uint32_t fat_sz32 = fat32_vol_fat_sz32();
	uint8_t  num_fats = fat32_vol_num_fats();
	uint32_t data_lba = fat32_vol_data_lba();
	uint8_t  spc      = fat32_vol_sec_per_clus();
	uint32_t tot_sec  = fat32_vol_tot_sec();

	repl_puts_P(PSTR("part_lba=")); repl_putu32(part_lba); repl_crlf();
	repl_puts_P(PSTR("fat_lba="));  repl_putu32(fat_lba);  repl_crlf();
	repl_puts_P(PSTR("fat_sz32=")); repl_putu32(fat_sz32); repl_crlf();
	repl_puts_P(PSTR("num_fats=")); repl_putu32(num_fats); repl_crlf();
	repl_puts_P(PSTR("data_lba=")); repl_putu32(data_lba); repl_crlf();
	repl_puts_P(PSTR("sec/clus=")); repl_putu32(spc);      repl_crlf();
	repl_puts_P(PSTR("tot_sec="));  repl_putu32(tot_sec);  repl_crlf();
	repl_puts_P(PSTR("size_mb="));  repl_putu32(tot_sec / 2048UL); repl_crlf();
}

static void cmd_init(void)
{
	if (sd_init() != SD_OK || fat32_mount() != 0)
		repl_puts_P(PSTR("!\r\n"));
	else
		repl_puts_P(PSTR("ok\r\n"));
}

static void cmd_sync(void)
{
	log_flush();
	repl_puts_P(PSTR("ok\r\n"));
}

static void cmd_reset(void)
{
	repl_puts_P(PSTR("reset\r\n"));
	wdt_enable(WDTO_15MS);
	for (;;)
		;
}

static void cmd_trim_free(void)
{
	int8_t ret;

	repl_puts_P(PSTR("trimming free...\r\n"));
	ret = fat32_trim_free();
	repl_puts_P(ret == 0 ? PSTR("ok\r\n") : PSTR("!\r\n"));
}

static void cmd_trim_full(void)
{
	repl_puts_P(PSTR("trimming data...\r\n"));
	fat32_trim_full();
	repl_puts_P(PSTR("ok\r\n"));
}

static void cmd_trim_fuller(void)
{
	repl_puts_P(PSTR("trimming card...\r\n"));
	fat32_trim_fuller();
	repl_puts_P(PSTR("ok\r\n"));
}

static void repl_dispatch(void)
{
	if      (line_eq("?"))           cmd_help();
	else if (line_eq("disk"))        cmd_disk();
	else if (line_eq("init"))        cmd_init();
	else if (line_eq("sync"))        cmd_sync();
	else if (line_eq("reset"))       cmd_reset();
	else if (line_eq("trim free"))   cmd_trim_free();
	else if (line_eq("trim fuller")) cmd_trim_fuller();
	else if (line_eq("trim full"))   cmd_trim_full();
	else                             repl_puts_P(PSTR("!\r\n"));
}

void repl_enter(void)
{
	cli();

	log_flush();

	repl_puts_P(PSTR("\r\n>"));

	for (;;) {
		repl_read_line();

		if (LINE_BUF[0] == 0) {
			repl_putc('>');
			continue;
		}

		repl_dispatch();
		repl_putc('>');
	}
}
