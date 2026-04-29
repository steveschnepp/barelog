/* platform_esp32s3_eeprom.c
 * 
 * Flash operations for ESP32-S3.
 * Provides eeprom_flash_erase_sector, eeprom_flash_write_bytes, eeprom_flash_read_bytes.
 * 
 * ESP32-S3 flash (SPI NOR):
 *   - 4 KB sectors (erase unit)
 *   - 256-byte pages (write unit)
 *   - XIP mapped at 0x42000000 (read only)
 *   - SPI flash controller (SPI0 or direct AHB)
 *
 * Note: Cannot write via XIP address, must use SPI flash controller
 * or direct memory mapping with cache invalidation.
 */

#include <stdint.h>
#include "platform_esp32s3_eeprom.h"

/* TODO: Define ESP32-S3 flash controller registers
 * Via SPI2 or direct flash AHB interface:
 * 
 * Flash commands:
 * #define FLASH_CMD_READ   0x03
 * #define FLASH_CMD_WRITE  0x02
 * #define FLASH_CMD_ERASE  0x20  (4KB sector erase)
 * #define FLASH_CMD_RDSR   0x05
 * #define FLASH_CMD_WRSR   0x01
 * #define FLASH_CMD_WREN   0x06
 * 
 * Typical flow uses SPI2 in direct mode (no DMA):
 * - Set command
 * - Set address
 * - Transfer data
 * - Poll status
 */

static void flash_wait_ready(void)
{
	/* TODO: Poll flash status register
	 * Send RDSR (0x05), read status byte
	 * Bit 0 = BUSY, poll until 0
	 */
}

void eeprom_flash_erase_sector(uint32_t addr)
{
	/* TODO: Erase 4 KB sector at addr
	 * 1. Send WREN (0x06)
	 * 2. Send ERASE (0x20) with 3-byte address (from XIP addr to flash offset)
	 * 3. Poll status until ready
	 * 
	 * Address conversion: XIP addr 0x42XXXXXX -> flash offset 0x00XXXXXX
	 */
	(void)addr;
}

void eeprom_flash_write_bytes(uint32_t addr, const uint8_t *data, uint16_t len)
{
	/* TODO: Write len bytes at addr
	 * Max 256 bytes per page program command
	 * 
	 * For each 256-byte page:
	 * 1. Send WREN (0x06)
	 * 2. Send WRITE (0x02) with 3-byte address
	 * 3. Send page bytes (up to 256)
	 * 4. Poll status until ready
	 * 
	 * Address conversion: XIP addr -> flash offset
	 */
	(void)addr;
	(void)data;
	(void)len;
}

void eeprom_flash_read_bytes(uint32_t addr, uint8_t *data, uint16_t len)
{
	/* TODO: Read len bytes from addr
	 * Two options:
	 * 1. XIP (Execute In Place) - simplest
	 *    XIP address is already memory-mapped, just memcpy
	 * 
	 * 2. SPI flash controller
	 *    Send READ (0x03) command + address
	 *    Read data via SPI
	 * 
	 * For simplicity, use XIP (read-only is safe)
	 */
	const uint8_t *src = (const uint8_t *)addr;
	uint16_t i;

	for (i = 0; i < len; i++)
		data[i] = src[i];
}
