/* platform_nrf52840_eeprom.c
 * 
 * Flash operations for nRF52840.
 * Provides eeprom_flash_erase_sector, eeprom_flash_write_bytes, eeprom_flash_read_bytes.
 * 
 * nRF52840 flash:
 *   - 4 KB sectors (erase unit)
 *   - 4-byte words (write unit, but can write at any alignment with proper padding)
 *   - NVMC (Non-Volatile Memory Controller) at 0x4001E000
 */

#include <stdint.h>
#include "platform_nrf52840_eeprom.h"

/* TODO: Define nRF52840 NVMC registers
 * #define NVMC_BASE       0x4001E000
 * #define NVMC_READY      (*(volatile uint32_t *)(NVMC_BASE + 0x400))
 * #define NVMC_CONFIG     (*(volatile uint32_t *)(NVMC_BASE + 0x504))
 * #define NVMC_ERASEPAGE  (*(volatile uint32_t *)(NVMC_BASE + 0x508))
 * #define NVMC_ERASEALL   (*(volatile uint32_t *)(NVMC_BASE + 0x50C))
 * 
 * #define NVMC_CONFIG_WEN  1  // Write enable
 * #define NVMC_CONFIG_EEN  2  // Erase enable
 */

static void nvmc_wait_ready(void)
{
	/* TODO: Poll NVMC_READY until ready
	 * while (!NVMC_READY);
	 */
}

void eeprom_flash_erase_sector(uint32_t addr)
{
	/* TODO: Erase 4 KB sector at addr
	 * 1. NVMC_CONFIG = NVMC_CONFIG_EEN (enable erase)
	 * 2. NVMC_ERASEPAGE = addr (erase address)
	 * 3. Wait for ready
	 * 4. NVMC_CONFIG = 0 (disable erase)
	 */
	(void)addr;
}

void eeprom_flash_write_bytes(uint32_t addr, const uint8_t *data, uint16_t len)
{
	/* TODO: Write len bytes at addr
	 * 1. NVMC_CONFIG = NVMC_CONFIG_WEN (enable write)
	 * 2. Copy data to flash address (byte-wise is OK)
	 *    volatile uint32_t *dst = (volatile uint32_t *)addr;
	 *    for (i = 0; i < len; i += 4)
	 *        dst[i/4] = *(uint32_t *)(data + i);
	 * 3. Wait for ready after each word (or batch)
	 * 4. NVMC_CONFIG = 0 (disable write)
	 */
	(void)addr;
	(void)data;
	(void)len;
}

void eeprom_flash_read_bytes(uint32_t addr, uint8_t *data, uint16_t len)
{
	/* TODO: Read len bytes from addr
	 * Flash is memory-mapped, simple memcpy
	 */
	const uint8_t *src = (const uint8_t *)addr;
	uint16_t i;

	for (i = 0; i < len; i++)
		data[i] = src[i];
}
