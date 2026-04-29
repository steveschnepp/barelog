#ifndef PLATFORM_CH32V203_EEPROM_H
#define PLATFORM_CH32V203_EEPROM_H

/* ================================================================
 * CH32V203 flash-based EEPROM
 * 
 * Flash: 64 KB total
 * Last 4 KB (0x0F000 - 0x0FFFF) reserved for EEPROM
 * 
 * Sector size: 256 bytes (CH32V203 uses 256B pages in 4KB blocks)
 * For simplicity, use 4 KB erase unit = EEPROM_FLASH_SIZE
 * ================================================================ */

#define EEPROM_FLASH_BASE_ADDR 0x0F000U
#define EEPROM_FLASH_SIZE      0x1000U

#endif /* PLATFORM_CH32V203_EEPROM_H */
