#include <stdint.h>
#include <avr/eeprom.h>
#include "config.h"

/*
 * Single source of truth for the prealloc size table.
 * fat32.c calls this instead of duplicating the switch.
 */
uint32_t config_prealloc_bytes(uint8_t code)
{
	switch (code) {
	case 0: return  1UL * 1024UL * 1024UL;
	case 1: return  4UL * 1024UL * 1024UL;
	case 2: return  8UL * 1024UL * 1024UL;
	case 3: return 16UL * 1024UL * 1024UL;
	case 4: return 32UL * 1024UL * 1024UL;
	default: return 8UL * 1024UL * 1024UL;
	}
}

void config_defaults(struct config *c)
{
	c->baud          = 115200UL;
	c->esc_char      = 0x1A;      /* CTRL+Z */
	c->esc_count     = 3;
	c->log_num       = 1;         /* first file is LOG00001.TXT */
	c->flags         = 0;
	c->prealloc_code = 2;         /* 8 MB default */
}

void config_save(const struct config *c)
{
	uint8_t *p;

	/* magic written first; a partial save is detectable on next boot */
	eeprom_write_dword((uint32_t *)EEPROM_OFF_MAGIC, EEPROM_MAGIC);
	eeprom_write_dword((uint32_t *)EEPROM_OFF_BAUD,  c->baud);

	p = (uint8_t *)EEPROM_OFF_ESC;
	eeprom_write_byte(p, c->esc_char);

	p = (uint8_t *)EEPROM_OFF_ESCCNT;
	eeprom_write_byte(p, c->esc_count);

	/* log_num stored as two bytes; avr-libc eeprom_write_word does
	 * not guarantee byte order on all toolchain versions, so we write
	 * explicitly little-endian to match the layout table. */
	p = (uint8_t *)EEPROM_OFF_LOGNUM;
	eeprom_write_byte(p,     (uint8_t)(c->log_num & 0xFF));
	eeprom_write_byte(p + 1, (uint8_t)(c->log_num >> 8));

	p = (uint8_t *)EEPROM_OFF_FLAGS;
	eeprom_write_byte(p, c->flags);

	p = (uint8_t *)EEPROM_OFF_PREALLOC;
	eeprom_write_byte(p, c->prealloc_code);

	/* EEPROM_OFF_REC_LO/HI are intentionally not written here.
	 * They are updated by log_flush() on every idle flush so that
	 * bytes_written survives a power loss mid-session. */
}

void config_load(struct config *c)
{
	uint32_t magic;
	uint8_t  lo, hi;
	uint8_t *p;

	magic = eeprom_read_dword((uint32_t *)EEPROM_OFF_MAGIC);
	if (magic != EEPROM_MAGIC) {
		/* first boot or EEPROM corruption: write and use defaults */
		config_defaults(c);
		config_save(c);
		return;
	}

	c->baud = eeprom_read_dword((uint32_t *)EEPROM_OFF_BAUD);

	p = (uint8_t *)EEPROM_OFF_ESC;
	c->esc_char = eeprom_read_byte(p);

	p = (uint8_t *)EEPROM_OFF_ESCCNT;
	c->esc_count = eeprom_read_byte(p);

	p = (uint8_t *)EEPROM_OFF_LOGNUM;
	lo = eeprom_read_byte(p);
	hi = eeprom_read_byte(p + 1);
	c->log_num = (uint16_t)lo | ((uint16_t)hi << 8);

	p = (uint8_t *)EEPROM_OFF_FLAGS;
	c->flags = eeprom_read_byte(p);

	p = (uint8_t *)EEPROM_OFF_PREALLOC;
	c->prealloc_code = eeprom_read_byte(p);
}
