# barelog — Flat Build System

Multi-platform firmware. Single directory, no subdirs. Everything fits in a gist.

## File layout

**Common sources** (all platforms):
```
config.c              config loading/saving
sd.c                  SD SPI protocol
fat32_state.c         FAT32 state + FAT I/O
fat32_mount.c         MBR/VBR parsing
fat32_log.c           log file open/resume/append
fat32_vol.c           volume layout getters
fat32_trim.c          TRIM operations
log.c                 ring drain, escape detection, flush
repl.c                command mode
main.c                init sequence, main loop
```

**Platform files** (one per chip, snake_case):
```
platform_atmega328p.c       UART, SPI, Timer, EEPROM, GPIO
platform_atmega328p.mk      avr-gcc, avrdude settings
platform_ch32v203.c         USB CDC, SPI, Timer, EEPROM, GPIO (skeleton)
platform_ch32v203.mk        riscv64 toolchain settings
platform_rp2040.c           USB CDC, SPI, Timer, EEPROM, GPIO (skeleton)
platform_rp2040.mk          arm-none-eabi toolchain settings
```

**Headers** (all platforms):
```
platform.h            Common interface, ring buffer inlines
uart.h                UART aliases
spi.h                 SPI interface
sd.h                  SD protocol
fat32.h               FAT32 public API
fat32_priv.h          FAT32 internal types
log.h                 logging loop interface
repl.h                command mode entry
config.h              config struct + interface
util.h                delay helpers
```

**Build artifacts**:
```
build_atmega328p/     AVR build outputs
build_ch32v203/       RISC-V build outputs
build_rp2040/         ARM Cortex-M0+ build outputs
```

## Usage

### Default (ATmega328P)

```sh
make                           # Build to build_atmega328p/barelog.hex
make size                      # Show sizes
make flash PORT=/dev/ttyUSB0   # Flash via avrdude
make clean                     # Clean all builds
```

### CH32V203

```sh
make PLATFORM=ch32v203         # Build to build_ch32v203/barelog.bin
make PLATFORM=ch32v203 size    # Show sizes
make PLATFORM=ch32v203 flash   # Flash via minichlink
```

### RP2040

```sh
make PLATFORM=rp2040           # Build to build_rp2040/barelog.bin
make PLATFORM=rp2040 size      # Show sizes
make PLATFORM=rp2040 flash     # Flash via openocd
```

## Makefile structure

**Main Makefile:**
- Includes `platform_$(PLATFORM).mk` 
- Defines common CFLAGS, LDFLAGS
- Compiles common sources + platform source
- Outputs to `build_$(PLATFORM)/`

**Platform .mk files:**
Export toolchain and flags:
- `CC`, `OBJCOPY`, `SIZE`, `FLASHER`
- `MCU`, `F_CPU`, `CFLAGS`, `LDFLAGS`, `FLASHER_FLAGS`
- `PLATFORM_SRCS` (the one .c file)
- `PORT` (for atmega328p only)

## Adding a new platform

1. Create `platform_newchip.mk`:
   ```makefile
   CC := <toolchain>-gcc
   OBJCOPY := <toolchain>-objcopy
   SIZE := <toolchain>-size
   FLASHER := <flasher-tool>
   
   MCU := newchip
   F_CPU := <frequency>
   PLATFORM_SRCS := platform_newchip.c
   
   CFLAGS += <flags>
   LDFLAGS += <flags>
   FLASHER_FLAGS := <args>
   ```

2. Create `platform_newchip.c`:
   - `uint8_t ring[]`, `ring_head`, `ring_tail`
   - `uart_init`, `uart_putc`, `uart_puts_P`, `uart_getc_poll` + ISR
   - `spi_init`, `spi_set_fast`, `spi_write_buf`, `spi_write_zeros`, `spi_read_buf`
   - `timer_init`, `timer_flush_pending`, `timer_clear_pending`, `timer_restart` + ISR
   - `eeprom_init`, `eeprom_read_byte`, `eeprom_write_byte`, `eeprom_update_byte`
   - `gpio_led_init`, `gpio_led_toggle`
   - `platform_init`

3. Update `platform.h`:
   - Add `#elif` for your architecture
   - Define SPI inlines or extern declarations

## Gist friendly

All files in root or prefixed. No subdirectories. Fits easily in a GitHub gist.

```sh
# Clone from gist
git clone <gist-url> barelog
cd barelog
make PLATFORM=atmega328p
```
