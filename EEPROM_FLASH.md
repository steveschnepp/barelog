# Flash-based EEPROM

Reusable EEPROM emulation using flash. Works on CH32V203 and RP2040.

## Files

**Common:**
- `eeprom_flash.h` — public interface
- `eeprom_flash.c` — implementation (platform-agnostic)

**CH32V203:**
- `platform_ch32v203_eeprom.h` — base address + size
- `platform_ch32v203_eeprom.c` — flash controller (TODO)

**RP2040:**
- `platform_rp2040_eeprom.h` — base address + size
- `platform_rp2040_eeprom.c` — SSI/flash (TODO)

## Interface

```c
/* Initialize from flash */
void eeprom_flash_init(void);

/* Read one byte (from RAM cache) */
uint8_t eeprom_read_byte(uint16_t addr);

/* Write one byte (to RAM cache) */
void eeprom_write_byte(uint16_t addr, uint8_t val);

/* Update one byte (to RAM cache, only if changed) */
void eeprom_update_byte(uint16_t addr, uint8_t val);

/* Flush RAM cache to flash */
void eeprom_flash_sync(void);
```

## How it works

**RAM cache:**
- 256 bytes in RAM
- Changes buffered here
- No immediate flash writes

**Flash layout:**
- One 4 KB sector per platform
- Multiple 256-byte "slots" within sector
- Each slot: 256 bytes EEPROM + 1 byte generation counter
- ~16 generations per sector before erase

**Wear leveling:**
- Each sync() writes to next free slot
- Generationbyte tracks newest (lowest value)
- When sector full, erase and restart

**Usage:**
```c
int main(void) {
    eeprom_flash_init();        // Load from flash into cache
    
    val = eeprom_read_byte(10); // Read from cache (fast)
    eeprom_write_byte(20, 42);  // Write to cache
    
    // Periodically:
    eeprom_flash_sync();        // Flush cache to flash
    
    // Or on shutdown:
    eeprom_flash_sync();
}
```

## Platform implementation checklist

### CH32V203

In `platform_ch32v203_eeprom.c`, implement:

1. `eeprom_flash_erase_sector(addr)`:
   - Unlock flash controller
   - Set sector erase bit
   - Start operation
   - Wait for completion
   - Lock controller

2. `eeprom_flash_write_bytes(addr, data, len)`:
   - Unlock flash controller
   - Write data in 256-byte page chunks
   - Each page: set addr, set write bit, transfer bytes, start
   - Wait for completion per page
   - Lock controller

3. `eeprom_flash_read_bytes(addr, data, len)`:
   - Flash is memory-mapped on CH32V203
   - Simple memcpy from address

Consult WCH CH32V203 datasheet for flash controller register definitions.

### RP2040

In `platform_rp2040_eeprom.c`, implement:

1. `eeprom_flash_erase_sector(addr)`:
   - Disable XIP caching (if enabled)
   - Send WRITE_ENABLE command via SSI
   - Send SECTOR_ERASE (0x20) + 3-byte address
   - Poll status register for completion
   - Re-enable XIP

2. `eeprom_flash_write_bytes(addr, data, len)`:
   - Disable XIP
   - For each 256-byte page:
     - Send WRITE_ENABLE
     - Send PAGE_PROGRAM (0x02) + address
     - Send page bytes
     - Poll status
   - Re-enable XIP

3. `eeprom_flash_read_bytes(addr, data, len)`:
   - Flash is XIP-mapped at 0x10000000
   - Simple memcpy

Consult Raspberry Pi RP2040 datasheet for SSI and flash interface details.

## Integration with config.c

Replace platform-specific EEPROM calls in config.c:

```c
// Old (AVR):
#include <avr/eeprom.h>
eeprom_write_byte(...);

// New (flash-based):
#include "eeprom_flash.h"
eeprom_write_byte(...);
```

Same function names, so no changes to config.c logic.

Add to main.c:

```c
main(void) {
    ...
    eeprom_flash_init();  // Initialize EEPROM from flash
    
    config_load(&cfg);    // Load config (uses eeprom_read_byte)
    
    ...
    
    // During logging:
    log_flush();
    eeprom_flash_sync();  // Persist recovery record to flash
}
```

## Wear leveling calculation

- Sector: 4 KB
- Slot: 256 B EEPROM + 1 B generation = 257 B
- Slots per sector: ~15 (4096 / 257 ≈ 15)
- Writes before erase: 15

At 1 second flush interval (typical):
- 15 writes × 100,000 cycles per flash cell = 1.5M seconds
- ~17 days of continuous logging

Much safer than single-location writes.

## Testing

```c
/* Check eeprom_flash_sync works */
eeprom_update_byte(16, 0x01);
eeprom_update_byte(17, 0x02);
eeprom_flash_sync();

/* Power cycle, reload */
eeprom_flash_init();
assert(eeprom_read_byte(16) == 0x01);
assert(eeprom_read_byte(17) == 0x02);
```
