#ifndef PLATFORM_NRF52840_EEPROM_H
#define PLATFORM_NRF52840_EEPROM_H

/* ================================================================
 * nRF52840 flash-based EEPROM
 * 
 * Flash: 1 MB (0x00000000 - 0x000FFFFF)
 * Last 4 KB reserved for EEPROM: 0x000FC000 - 0x000FFFFF
 * 
 * Sector size: 4 KB (nRF52840 erase unit)
 * ================================================================ */

#define EEPROM_FLASH_BASE_ADDR 0x000FC000U
#define EEPROM_FLASH_SIZE      0x4000U

#endif /* PLATFORM_NRF52840_EEPROM_H */
