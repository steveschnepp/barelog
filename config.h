#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/*
 * EEPROM layout (all multi-byte fields little-endian):
 *
 *   offset  0 : 4 B  magic 0x4F4C3200  — detects first boot / corruption
 *   offset  4 : 4 B  baud rate (uint32)
 *   offset  8 : 1 B  escape character (default 0x1A = CTRL+Z)
 *   offset  9 : 1 B  escape count (default 3)
 *   offset 10 : 2 B  log file number — incremented on each new file
 *   offset 12 : 1 B  flags (see CFG_FL_*)
 *   offset 13 : 1 B  prealloc size code (see config_prealloc_bytes)
 *   offset 14 : 2 B  reserved, written as 0xFF
 *   offset 16 : 2 B  recovery: bytes_written bits [15:0]
 *   offset 18 : 2 B  recovery: bytes_written bits [31:16]
 *
 * The recovery fields are written by log_flush(), not by config_save().
 * They allow resuming an interrupted write to an existing pre-allocated
 * file after an unclean power loss.
 */

#define EEPROM_MAGIC        0x4F4C3200UL
#define EEPROM_OFF_MAGIC    0
#define EEPROM_OFF_BAUD     4
#define EEPROM_OFF_ESC      8
#define EEPROM_OFF_ESCCNT   9
#define EEPROM_OFF_LOGNUM   10
#define EEPROM_OFF_FLAGS    12
#define EEPROM_OFF_PREALLOC 13
#define EEPROM_OFF_REC_LO   16
#define EEPROM_OFF_REC_HI   18

/* flags byte bits */
/* bit 0: do not treat RX-low at boot as a factory reset trigger */
#define CFG_FL_IGNORE_RX_RST  (1 << 0)

/*
 * prealloc_code encodes the pre-allocated log file size:
 *   0 =  1 MB
 *   1 =  4 MB
 *   2 =  8 MB  (default)
 *   3 = 16 MB
 *   4 = 32 MB
 *
 * The file is pre-allocated and zero-filled at open time so that
 * subsequent sector writes never need to extend the FAT chain mid-stream.
 */

struct config {
	uint32_t baud;          /* UART baud rate */
	uint8_t  esc_char;      /* escape character value */
	uint8_t  esc_count;     /* number of consecutive escapes to trigger */
	uint16_t log_num;       /* next log file number (LOGnnnnn.TXT) */
	uint8_t  flags;         /* CFG_FL_* bitmask */
	uint8_t  prealloc_code; /* index into prealloc size table */
};

/*
 * Convert a prealloc_code to bytes.
 * Single definition shared by config.c and fat32.c.
 * Returns 8 MB for any unknown code.
 */
uint32_t config_prealloc_bytes(uint8_t code);

/*
 * Load config from EEPROM into *c.
 * If magic is missing or corrupt, writes factory defaults first.
 */
void config_load(struct config *c);

/*
 * Write all config fields to EEPROM.
 * Does not write the recovery fields (EEPROM_OFF_REC_LO/HI);
 * those are maintained by log_flush().
 */
void config_save(const struct config *c);

/*
 * Fill *c with factory defaults. Does not touch EEPROM.
 */
void config_defaults(struct config *c);

#endif /* CONFIG_H */
