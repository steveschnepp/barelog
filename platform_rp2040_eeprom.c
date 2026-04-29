/* platform_rp2040_eeprom.c
 * 
 * Flash controller operations for RP2040.
 * Uses SSI (Serial Serial Interface) for flash access via QSPI.
 * 
 * RP2040 flash (via SSI):
 *   - 4 KB sectors (erase unit)
 *   - 256-byte pages (optimal write size)
 *   - XIP (Execute In Place) at 0x10000000
 *   - SSI_BASE at 0x18000000
 */

#include <stdint.h>
#include "platform_rp2040_eeprom.h"

/* TODO: Define RP2040 SSI/flash registers
 * #define SSI_BASE         0x18000000
 * #define SSI_CTRLR0       (*(volatile uint32_t *)(SSI_BASE + 0x00))
 * #define SSI_CTRLR1       (*(volatile uint32_t *)(SSI_BASE + 0x04))
 * #define SSI_DR0          (*(volatile uint32_t *)(SSI_BASE + 0x60))
 * 
 * Flash commands:
 * #define FLASH_CMD_READ   0x03
 * #define FLASH_CMD_WRITE  0x02
 * #define FLASH_CMD_ERASE  0x20  (4KB sector erase)
 * #define FLASH_CMD_RDSR   0x05  (read status register)
 * 
 * Typical flow:
 * 1. Send command + address
 * 2. Wait for ready (status bit 0)
 * 3. Data transfer
 * 4. Disable XIP before erase/write
 */

static void flash_wait_ready(void)
{
	/* TODO: Poll flash status register
	 * Send RDSR (0x05) command, read status
	 * Bit 0 = busy, poll until 0
	 */
}

void eeprom_flash_erase_sector(uint32_t addr)
{
	/* TODO: Erase 4 KB sector at addr
	 * 1. Disable XIP caching (if needed)
	 * 2. Send WRITE_ENABLE (0x06) command
	 * 3. Send ERASE (0x20) + 3-byte address
	 * 4. Poll status until ready
	 * 5. Re-enable XIP
	 * 
	 * XIP disable: clear bit in XIP_CTRL or similar
	 */
	(void)addr;
}

void eeprom_flash_write_bytes(uint32_t addr, const uint8_t *data, uint16_t len)
{
	/* TODO: Write len bytes at addr
	 * Flash requires page-aligned writes (256-byte pages)
	 * 
	 * 1. Disable XIP
	 * 2. For each 256-byte chunk:
	 *    a. Send WRITE_ENABLE
	 *    b. Send WRITE (0x02) + 3-byte address
	 *    c. Send len bytes
	 *    d. Poll status
	 * 3. Re-enable XIP
	 */
	(void)addr;
	(void)data;
	(void)len;
}

void eeprom_flash_read_bytes(uint32_t addr, uint8_t *data, uint16_t len)
{
	/* TODO: Read len bytes from addr
	 * RP2040 flash is XIP-mapped at 0x10000000
	 * Just memcpy from the address in RAM space
	 * 
	 * If addr is already XIP-mapped, simple memcpy works:
	 */
	const uint8_t *src = (const uint8_t *)addr;
	uint16_t i;

	for (i = 0; i < len; i++)
		data[i] = src[i];
}
