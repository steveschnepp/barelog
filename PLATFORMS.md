# barelog — Five Platforms

Flat build system supporting ATmega328P, CH32V203, RP2040, nRF52840, ESP32-S3.

## Platform summary

| Platform | MCU | Clock | USB | Flash | RAM | Transport | Notes |
|----------|-----|-------|-----|-------|-----|-----------|-------|
| atmega328p | AVR 328P | 16 MHz | - | 32 KB | 2 KB | UART only | Bootloader: Optiboot |
| ch32v203 | RISC-V | 144 MHz | native | 64 KB | 20 KB | USB CDC | Bootloader: built-in |
| rp2040 | Cortex-M0+ | 125 MHz | native | 2 MB | 256 KB | USB CDC | Boot ROM: uf2 drag-drop |
| nrf52840 | Cortex-M4 | 64 MHz | native | 1 MB | 256 KB | USB CDC | Bootloader: Segger JLink |
| esp32s3 | Dual Xtensa | 240 MHz | native | 4-16 MB | 512 KB + PSRAM | USB CDC | Bootloader: ROM, esptool.py |

## Build commands

```sh
# ATmega328P (default)
make
make flash PORT=/dev/ttyUSB0

# CH32V203
make PLATFORM=ch32v203
make PLATFORM=ch32v203 flash

# RP2040
make PLATFORM=rp2040
make PLATFORM=rp2040 flash

# nRF52840
make PLATFORM=nrf52840
make PLATFORM=nrf52840 flash

# ESP32-S3
make PLATFORM=esp32s3
make PLATFORM=esp32s3 flash

# All
make clean
```

## File structure (flat, gist-friendly)

```
Makefile
platform_atmega328p.mk      platform_atmega328p.c
platform_ch32v203.mk        platform_ch32v203.c
platform_rp2040.mk          platform_rp2040.c
platform_nrf52840.mk        platform_nrf52840.c
platform_esp32s3.mk         platform_esp32s3.c

platform_ch32v203_eeprom.h  platform_ch32v203_eeprom.c
platform_rp2040_eeprom.h    platform_rp2040_eeprom.c
platform_nrf52840_eeprom.h  platform_nrf52840_eeprom.c
platform_esp32s3_eeprom.h   platform_esp32s3_eeprom.c

platform.h   uart.h   spi.h   config.h   fat32.h   log.h   repl.h   util.h

config.c   sd.c   fat32_*.c   log.c   repl.c   main.c

eeprom_flash.h   eeprom_flash.c

build_atmega328p/
build_ch32v203/
build_rp2040/
build_nrf52840/
build_esp32s3/
```

## Platform status

| Platform | UART/USB | SPI | Timer | EEPROM | GPIO | Status |
|----------|----------|-----|-------|--------|------|--------|
| atmega328p | UART ✓ | ✓ | ✓ | AVR ✓ | ✓ | Complete |
| ch32v203 | USB CDC (TODO) | TODO | TODO | Flash (TODO) | TODO | Skeleton |
| rp2040 | USB CDC (TODO) | TODO | TODO | Flash (TODO) | TODO | Skeleton |
| nrf52840 | USB CDC (TODO) | TODO | TODO | Flash (TODO) | TODO | Skeleton |
| esp32s3 | USB CDC (TODO) | TODO | TODO | Flash (TODO) | TODO | Skeleton |

## Implementation checklist

For each new platform, implement:

### platform_CHIP.mk
- Toolchain (CC, OBJCOPY, SIZE, FLASHER)
- MCU, F_CPU
- CFLAGS, LDFLAGS
- FLASHER_FLAGS
- PLATFORM_SRCS

### platform_CHIP.c
- ring[], ring_head, ring_tail
- uart_init, uart_putc, uart_puts_P, uart_getc_poll + ISR
- spi_init, spi_set_fast, spi_write_buf, spi_write_zeros, spi_read_buf
- timer_init, timer_flush_pending, timer_clear_pending, timer_restart + ISR
- eeprom_init (stub, delegated to eeprom_flash.c)
- gpio_led_init, gpio_led_toggle
- platform_init

### platform.h
- Add #elif block for chip detection
- Define SPI inlines: spi_transfer, spi_cs_assert, spi_cs_release (or extern)

### platform_CHIP_eeprom.h
- EEPROM_FLASH_BASE_ADDR (last 4 KB of flash)
- EEPROM_FLASH_SIZE (typically 0x1000)

### platform_CHIP_eeprom.c
- eeprom_flash_erase_sector(addr) - erase 4 KB sector
- eeprom_flash_write_bytes(addr, data, len) - write bytes to flash
- eeprom_flash_read_bytes(addr, data, len) - read bytes from flash

## Testing

For each platform, verify:

1. **Compile**
   ```sh
   make PLATFORM=CHIP
   ```

2. **Size**
   ```sh
   make PLATFORM=CHIP size
   ```

3. **Flash**
   ```sh
   make PLATFORM=CHIP flash
   ```

4. **Boot sequence**
   - Monitor UART/USB CDC
   - Expect: '1' '2' '<' on successful boot
   - Check LED blink on RX activity

5. **Logging**
   - Send data via UART/USB
   - Check SD card for LOGnnnnn.TXT
   - Verify data integrity

6. **Command mode**
   - Send escape sequence (CTRL+Z × 3)
   - Try commands: `?`, `disk`, `sync`, `trim free`

## Notes

- **ATmega328P** — reference implementation, fully working
- **CH32V203, RP2040, nRF52840, ESP32-S3** — skeletons with TODO comments
- Each platform needs USB CDC driver implementation (most complex part)
- SPI and Timer are straightforward per chip datasheet
- Flash EEPROM abstraction works identically on all platforms

## Next steps

1. Implement USB CDC for one target (start with nRF52840 or RP2040)
2. Test logging and recovery
3. Port remaining platforms
4. Validate command mode on each
5. Real-world SD card testing
