# barelog Build System

Multi-platform firmware build with platform-specific includes.

## Directory layout

```
.
├── Makefile                    # Main build driver
├── platform/
│   ├── atmega328p.mk          # AVR-specific settings
│   ├── atmega328p.c           # Platform impl (UART, SPI, Timer, EEPROM, GPIO)
│   ├── ch32v203.mk            # CH32V203 settings (skeleton)
│   ├── ch32v203.c             # Platform impl (skeleton)
│   ├── rp2040.mk              # RP2040 settings (skeleton)
│   └── rp2040.c               # Platform impl (skeleton)
├── include/
│   ├── platform.h             # Common platform interface
│   ├── uart.h                 # UART aliases to ring
│   ├── spi.h                  # SPI interface
│   └── *.h                    # Other headers
├── src/
│   ├── main.c
│   ├── config.c
│   ├── log.c
│   ├── repl.c
│   ├── sd.c
│   ├── fat32_*.c
│   └── *.c                    # Other sources
└── build/
    ├── atmega328p/
    │   ├── *.o
    │   ├── barelog.elf
    │   ├── barelog.hex
    │   └── barelog.map
    ├── ch32v203/
    └── rp2040/
```

## Build commands

### ATmega328P (default)

```sh
# Build
make

# Build and show size
make size

# Build and flash to /dev/ttyUSB0
make flash PORT=/dev/ttyUSB0

# Clean
make clean
```

### CH32V203

```sh
# Build for CH32V203
make PLATFORM=ch32v203

# Flash via minichlink (SWIO on PD1)
make PLATFORM=ch32v203 flash

# Clean CH32V203 build only
make clean
```

### RP2040

```sh
# Build for RP2040
make PLATFORM=rp2040

# Flash via OpenOCD (requires CMSIS-DAP adapter)
make PLATFORM=rp2040 flash

# Clean RP2040 build only
make clean
```

## Makefile structure

**Main Makefile:**
- Defines PLATFORM variable (default: atmega328p)
- Includes platform/$(PLATFORM).mk
- Defines common sources
- Builds platform sources + common sources
- Provides size, flash, clean targets

**Platform .mk files (atmega328p.mk, etc):**
- Exports: CC, OBJCOPY, SIZE, FLASHER
- Exports: MCU, F_CPU, CFLAGS, LDFLAGS, FLASHER_FLAGS
- Defines: PLATFORM_SRCS (platform .c file)
- Defines: PORT (for atmega328p)

**Separate build dirs per platform:**
- `build/atmega328p/` → ARM AVR build
- `build/ch32v203/` → RISC-V CH32 build
- `build/rp2040/` → ARM Cortex-M0+ build

No object file collisions. Clean is fast.

## Adding a new platform

1. Create `platform/newchip.mk`:
   ```makefile
   CC := <toolchain-prefix>-gcc
   OBJCOPY := <toolchain-prefix>-objcopy
   SIZE := <toolchain-prefix>-size
   FLASHER := <flasher-tool>
   
   MCU := newchip
   F_CPU := <frequency>
   PLATFORM_SRCS := platform/newchip.c
   
   CFLAGS += <cpu-specific-flags>
   LDFLAGS += <cpu-specific-flags>
   FLASHER_FLAGS := <flasher-args>
   ```

2. Create `platform/newchip.c`:
   - Define ring[], ring_head, ring_tail
   - Implement uart_init, uart_putc, uart_puts_P, uart_getc_poll
   - Implement spi_init, spi_set_fast, spi_write_buf, spi_write_zeros, spi_read_buf
   - Implement timer_init, timer_flush_pending, timer_clear_pending, timer_restart
   - Implement eeprom_init, eeprom_read_byte, eeprom_write_byte, eeprom_update_byte
   - Implement gpio_led_init, gpio_led_toggle
   - Implement platform_init
   - Define ISRs (UART RX, Timer)
   - Define spi_transfer, spi_cs_assert, spi_cs_release (inlines in platform.h or functions)

3. Update `platform/platform.h`:
   - Add platform-specific register macros
   - Add inline spi_transfer, spi_cs_assert/release if needed

4. Test:
   ```sh
   make PLATFORM=newchip
   ```

## Parallel builds

Build multiple platforms at once without conflicts:

```sh
make PLATFORM=atmega328p &
make PLATFORM=ch32v203 &
make PLATFORM=rp2040 &
wait
```

Each uses its own `build/<platform>/` directory.
