#ifndef PLATFORM_ESP32S3_EEPROM_H
#define PLATFORM_ESP32S3_EEPROM_H

/* ================================================================
 * ESP32-S3 flash-based EEPROM
 * 
 * Flash: 4-16 MB (typically 8 MB)
 * Memory map (typical):
 *   0x0000000 - 0x00FFFFF : Bootloader + partition table
 *   0x0100000 - 0x0FFFFF  : App code
 *   0x1000000 - 0x7FFFFF  : Reserved/SPIFFS/data
 * 
 * Use a dedicated 4 KB partition (via partition table) or
 * reserve last 4 KB: 0x7FF000 - 0x7FFFFF (for 8 MB flash)
 * 
 * Address here is the XIP (Execute In Place) mapped address
 * (0x42000000 in memory space for flash XIP)
 * ================================================================ */

#define EEPROM_FLASH_BASE_ADDR 0x42FF000U  /* XIP mapped, 8 MB flash, last 4 KB */
#define EEPROM_FLASH_SIZE      0x1000U

#endif /* PLATFORM_ESP32S3_EEPROM_H */
