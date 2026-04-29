#ifndef EEPROM_FLASH_H
#define EEPROM_FLASH_H

#include <stdint.h>

/* ================================================================
 * Flash-based EEPROM emulation
 * 
 * Works on any platform with flash. Provides standard EEPROM interface.
 * Platform must define:
 *   - EEPROM_FLASH_BASE_ADDR: start of EEPROM sector
 *   - EEPROM_FLASH_SIZE: sector size in bytes (e.g., 4096)
 *   - eeprom_flash_erase_sector(addr): erase 4KB sector at addr
 *   - eeprom_flash_write_bytes(addr, data, len): write to flash
 *   - eeprom_flash_read_bytes(addr, data, len): read from flash
 * 
 * See platform_ch32v203_eeprom.h and platform_rp2040_eeprom.h for examples.
 * ================================================================ */

/*
 * Read one byte from EEPROM.
 * Returns value from RAM cache; syncs from flash on init.
 */
uint8_t eeprom_read_byte(uint16_t addr);

/*
 * Write one byte to EEPROM.
 * Updates RAM immediately; flash write deferred until eeprom_flash_sync().
 */
void eeprom_write_byte(uint16_t addr, uint8_t val);

/*
 * Update one byte if different.
 * Only writes to flash if value changed; reduces wear.
 */
void eeprom_update_byte(uint16_t addr, uint8_t val);

/*
 * Initialize EEPROM from flash.
 * Call once at startup before any read/write.
 */
void eeprom_flash_init(void);

/*
 * Sync RAM cache to flash.
 * Call after a batch of updates, or periodically to avoid data loss.
 * Handles wear leveling: erases sector if needed, uses write-ahead.
 */
void eeprom_flash_sync(void);

/*
 * Platform-provided functions (to be implemented per chip).
 * These are called by eeprom_flash.c only.
 */

extern void eeprom_flash_erase_sector(uint32_t addr);
extern void eeprom_flash_write_bytes(uint32_t addr, const uint8_t *data, uint16_t len);
extern void eeprom_flash_read_bytes(uint32_t addr, uint8_t *data, uint16_t len);

#endif /* EEPROM_FLASH_H */
