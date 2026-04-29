#ifndef PLATFORM_RP2040_EEPROM_H
#define PLATFORM_RP2040_EEPROM_H

/* ================================================================
 * RP2040 flash-based EEPROM
 * 
 * Flash: 2 MB total (at 0x10000000 in XIP space)
 * Bootloader: 64 KB (0x10000000 - 0x1000FFFF, don't touch)
 * App code: typically first 256 KB
 * Last 4 KB reserved for EEPROM: 0x101FF000 - 0x101FFFFF
 * 
 * Sector size: 4 KB (RP2040 erase unit)
 * ================================================================ */

#define EEPROM_FLASH_BASE_ADDR 0x10000000U + (2 * 1024 * 1024) - (4 * 1024)
#define EEPROM_FLASH_SIZE      0x1000U

#endif /* PLATFORM_RP2040_EEPROM_H */
