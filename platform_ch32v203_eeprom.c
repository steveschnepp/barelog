/* platform_ch32v203_eeprom.c
 * 
 * Flash controller operations for CH32V203.
 * Provides eeprom_flash_erase_sector, eeprom_flash_write_bytes, eeprom_flash_read_bytes.
 * 
 * CH32V203 flash:
 *   - 256-byte pages (write granule)
 *   - 4 KB blocks (erase granule)
 *   - FLASH_CTLR: control register
 *   - FLASH_ADDR: address register
 *   - FLASH_DATA: data register
 */

#include <stdint.h>
#include "platform_ch32v203_eeprom.h"

/* TODO: Define CH32V203 flash controller registers
 * #define FLASH_BASE       0x40022000
 * #define FLASH_CTLR       (*(volatile uint32_t *)(FLASH_BASE + 0x00))
 * #define FLASH_ADDR       (*(volatile uint32_t *)(FLASH_BASE + 0x04))
 * #define FLASH_DATA       (*(volatile uint32_t *)(FLASH_BASE + 0x08))
 * 
 * #define FLASH_CTLR_BUSY  (1 << 0)
 * #define FLASH_CTLR_LOCK  (1 << 7)
 * #define FLASH_CTLR_SER   (1 << 1)  // sector erase
 * #define FLASH_CTLR_PG    (1 << 0)  // page program
 * #define FLASH_CTLR_STRT  (1 << 6)  // start erase/write
 */

static void flash_wait_busy(void)
{
	/* TODO: Poll FLASH_CTLR_BUSY until clear
	 * while (FLASH_CTLR & FLASH_CTLR_BUSY)
	 *	;
	 */
}

static void flash_unlock(void)
{
	/* TODO: Unlock flash controller
	 * Write 0x45670123 to FLASH_KEY, then 0xCDEF89AB
	 * or check datasheet for exact sequence
	 */
}

static void flash_lock(void)
{
	/* TODO: Lock flash controller
	 * Set FLASH_CTLR_LOCK bit
	 */
}

void eeprom_flash_erase_sector(uint32_t addr)
{
	/* TODO: Erase 4 KB sector at addr
	 * 1. Unlock flash
	 * 2. Set FLASH_ADDR = addr
	 * 3. Set FLASH_CTLR_SER bit in FLASH_CTLR
	 * 4. Set FLASH_CTLR_STRT to start
	 * 5. Wait for busy
	 * 6. Clear SER bit
	 * 7. Lock flash
	 */
	(void)addr;
}

void eeprom_flash_write_bytes(uint32_t addr, const uint8_t *data, uint16_t len)
{
	/* TODO: Write len bytes at addr (must be 256-byte aligned within page)
	 * 1. Unlock flash
	 * 2. For each 256-byte page:
	 *    a. Set FLASH_ADDR = page_addr
	 *    b. Set FLASH_CTLR_PG bit
	 *    c. Write 256 bytes to FLASH_DATA (32-bit at a time)
	 *    d. Set FLASH_CTLR_STRT to start
	 *    e. Wait for busy
	 * 3. Clear PG bit
	 * 4. Lock flash
	 */
	(void)addr;
	(void)data;
	(void)len;
}

void eeprom_flash_read_bytes(uint32_t addr, uint8_t *data, uint16_t len)
{
	/* TODO: Read len bytes from addr
	 * Flash is memory-mapped, so just memcpy from addr
	 */
	const uint8_t *src = (const uint8_t *)addr;
	uint16_t i;

	for (i = 0; i < len; i++)
		data[i] = src[i];
}
