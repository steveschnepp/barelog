# barelog — Complete Project Summary

Multi-platform bare-metal SD card data logger. Five MCU targets, flat build system, flash-based EEPROM, Docker CI/CD.

## Project overview

**barelog** is a zero-dependency data logger that captures UART/USB data to microSD card. Runs on five platforms:

- ATmega328P (UART, reference implementation)
- CH32V203 (USB CDC)
- RP2040 (USB CDC)
- nRF52840 (USB CDC)
- ESP32-S3 (USB CDC)

## Directory structure (flat, gist-friendly)

```
# Makefiles
Makefile                     # Main build driver
platform_atmega328p.mk      # AVR toolchain settings
platform_ch32v203.mk        # RISC-V toolchain settings
platform_rp2040.mk          # ARM Cortex-M0+ settings
platform_nrf52840.mk        # ARM Cortex-M4 settings
platform_esp32s3.mk         # Xtensa settings

# Platform implementations
platform_atmega328p.c       # UART, SPI, Timer, EEPROM, GPIO (complete)
platform_ch32v203.c         # USB CDC, SPI, Timer, EEPROM, GPIO (skeleton)
platform_rp2040.c           # USB CDC, SPI, Timer, EEPROM, GPIO (skeleton)
platform_nrf52840.c         # USB CDC, SPI, Timer, EEPROM, GPIO (skeleton)
platform_esp32s3.c          # USB CDC, SPI, Timer, EEPROM, GPIO (skeleton)

# EEPROM (flash-based for non-AVR platforms)
eeprom_flash.h              # Common EEPROM abstraction
eeprom_flash.c              # Wear-leveled flash EEPROM implementation
platform_ch32v203_eeprom.h  # CH32V203 flash params
platform_ch32v203_eeprom.c  # CH32V203 flash operations
platform_rp2040_eeprom.h    # RP2040 flash params
platform_rp2040_eeprom.c    # RP2040 flash operations
platform_nrf52840_eeprom.h  # nRF52840 flash params
platform_nrf52840_eeprom.c  # nRF52840 flash operations
platform_esp32s3_eeprom.h   # ESP32-S3 flash params
platform_esp32s3_eeprom.c   # ESP32-S3 flash operations

# Common source files (all platforms)
main.c                      # Init sequence, main loop
config.c                    # EEPROM config load/save
sd.c                        # SD SPI protocol
fat32_state.c               # FAT32 state, FAT entry I/O
fat32_mount.c               # MBR/VBR parsing
fat32_log.c                 # Log file open/resume/append
fat32_vol.c                 # Volume layout getters
fat32_trim.c                # TRIM operations
log.c                       # Ring drain, escape detection, idle flush
repl.c                      # Command mode UART loop

# Headers
platform.h                  # Platform abstraction interface
uart.h                      # UART aliases
spi.h                       # SPI interface
sd.h                        # SD protocol
fat32.h                     # FAT32 public API
fat32_priv.h                # FAT32 internal types
log.h                       # Logging interface
repl.h                      # Command mode entry
config.h                    # Config struct
util.h                      # Delay helpers

# Docker
Dockerfile_optimized        # Multi-stage build, all platforms, ~2.5 GB
Dockerfile.atmega328p       # AVR only, ~500 MB
Dockerfile.arm              # ARM only (RP2040, nRF52840), ~700 MB
Dockerfile.riscv            # RISC-V only (CH32V203), ~600 MB
Dockerfile.esp32s3          # ESP32-S3 only, ~1.2 GB
docker-compose.yml          # Compose config for all images
.dockerignore               # Exclude files from build context

# Scripts
build_all.sh                # Build all platforms with Docker
.github_workflows_ci.yml    # GitHub Actions CI/CD

# Documentation
README.md                   # Original OpenLog firmware overview
BUILD.md                    # Build system documentation (old, nested)
BUILD_flat.md               # Build system documentation (current)
EEPROM_FLASH.md             # Flash-based EEPROM guide
PLATFORMS.md                # Multi-platform overview and status
DOCKER.md                   # Docker usage guide
DOCKER_QUICK_REF.md         # Docker command reference

# Build artifacts (generated)
build_atmega328p/           # AVR build outputs
build_ch32v203/             # RISC-V build outputs
build_rp2040/               # ARM Cortex-M0+ outputs
build_nrf52840/             # ARM Cortex-M4 outputs
build_esp32s3/              # Xtensa outputs
```

## Key features

### Logging

- 1024-byte ring buffer (UART RX interrupt)
- Zero-copy 512-byte sector writes to SD
- Escape sequence detection (CTRL+Z × 3)
- Pre-allocated log files (configurable 1-32 MB)
- Recovery from power loss (EEPROM-backed state)

### Command mode

After escape sequence:
- `?` — help
- `disk` — volume layout
- `init` — re-run SD init
- `sync` — flush and update directory
- `reset` — reboot
- `trim free` — erase unallocated clusters
- `trim full` — erase data region
- `trim fuller` — erase entire card

### FAT32 support

- File pre-allocation and zero-fill
- Wear-leveled resume from EEPROM
- Cluster chain extension on overflow
- Multi-FAT consistency
- Dir entry updates for size tracking

### EEPROM (platform-specific)

**ATmega328P:** Native AVR EEPROM (built-in)

**Other platforms:** Flash-based emulation
- 256-byte cache in RAM
- Lazy writes to flash
- Wear leveling (~16 generations per 4 KB sector)
- 0xFF generation bytes for power-loss safety

## Build system

**Flat Makefile with includes:**
```sh
make                        # atmega328p (default)
make PLATFORM=ch32v203      # CH32V203
make PLATFORM=rp2040        # RP2040
make PLATFORM=nrf52840      # nRF52840
make PLATFORM=esp32s3       # ESP32-S3
make clean                  # Clean all builds
```

Each platform has own build dir: `build_$(PLATFORM)/`

## Platform status

| Platform | Status | Docs | Transport |
|----------|--------|------|-----------|
| ATmega328P | Complete | ✓ | UART |
| CH32V203 | Skeleton (TODO) | ✓ | USB CDC |
| RP2040 | Skeleton (TODO) | ✓ | USB CDC |
| nRF52840 | Skeleton (TODO) | ✓ | USB CDC |
| ESP32-S3 | Skeleton (TODO) | ✓ | USB CDC |

All skeletons have detailed TODO comments for register-level code.

## Docker

**Multi-stage images (Debian 13 trixie):**

| Name | Size | Platforms |
|------|------|-----------|
| barelog:latest | 2.5 GB | All 5 |
| barelog:avr | 500 MB | ATmega328P |
| barelog:arm | 700 MB | RP2040, nRF52840 |
| barelog:riscv | 600 MB | CH32V203 |
| barelog:esp32s3 | 1.2 GB | ESP32-S3 |

**Usage:**
```sh
# Build image
docker build -f Dockerfile_optimized -t barelog:latest .

# Run build
docker run -it --rm -v $(pwd):/workspace barelog:latest \
  make PLATFORM=rp2040

# Or with docker-compose
docker-compose run --rm barelog make PLATFORM=atmega328p
```

## Implementation checklist

### For each new platform

1. **platform_CHIP.mk** — Toolchain + flags
2. **platform_CHIP.c** — UART/USB, SPI, Timer, EEPROM, GPIO
3. **platform_CHIP_eeprom.h/c** — Flash operations (if not AVR)
4. **platform.h** — Add architecture detection + SPI inlines

Common code (main, log, config, fat32, sd, repl) works identically.

### Implementation order

1. GPIO LED init/toggle (simplest, verify toolchain works)
2. UART or USB CDC (debug output)
3. SPI init + transfer (SD communication)
4. Timer + ISR (idle flush)
5. Flash EEPROM (recovery state)
6. Test boot sequence: `1` `2` `<`
7. Test logging with escape sequence
8. Test command mode

## Files and LOC (approximate)

| File | Type | Lines | Notes |
|------|------|-------|-------|
| platform_atmega328p.c | C | 250 | Complete |
| platform_*.c (ch32v203, rp2040, etc) | C | ~200 | Skeletons |
| eeprom_flash.c | C | 150 | Shared impl |
| main.c | C | 120 | Same for all |
| log.c | C | 180 | Same for all |
| repl.c | C | 350 | Same for all |
| fat32_*.c | C | ~1000 | Shared impl |
| sd.c | C | ~400 | Same for all |
| config.c | C | ~80 | Same for all |
| Headers | H | ~300 | Interfaces |
| Makefiles | mk | ~100 | Per platform |
| Docker | Dockerfile | ~200 | Recipes |
| Scripts | sh | ~50 | CI/CD |

**Total:** ~3500 lines C, ~300 lines build/docker

## Documentation files

- `README.md` — Original OpenLog overview
- `BUILD_flat.md` — Makefile structure and usage
- `EEPROM_FLASH.md` — Flash EEPROM design and integration
- `PLATFORMS.md` — Multi-platform status and checklist
- `DOCKER.md` — Docker usage guide
- `DOCKER_QUICK_REF.md` — Docker command reference

## Getting started

### Local development (native toolchain)

```sh
# ATmega328P
sudo apt-get install gcc-avr avr-libc avrdude
make
make flash PORT=/dev/ttyUSB0
```

### Docker (any platform)

```sh
# Build all platforms
docker build -f Dockerfile_optimized -t barelog:latest .

# Run build
docker run -it --rm -v $(pwd):/workspace barelog:latest bash

# Inside: make PLATFORM=rp2040
```

### GitHub Actions

Push to main, GitHub automatically builds all platforms.

## Next steps

1. Implement USB CDC on one platform (start with RP2040 or nRF52840)
2. Test full logging cycle on hardware
3. Verify recovery from power loss
4. Test command mode on each platform
5. Real-world SD card validation
6. Performance tuning (write speed, power draw)

## Notes

- No dynamic allocation, no external libraries
- C99 with Linux kernel code style
- Minimal ISR overhead (ISR only updates head/flags)
- Zero-copy sector writes (ring to SD direct)
- Wear-leveled EEPROM (extends flash lifetime)
- Flat file layout (gist-friendly)

## Reference

- **OpenLog protocol:** SparkFun original firmware
- **FAT32:** Microsoft specification
- **SD SPI:** PhysicalLayer.com SPI mode guide
- **Datasheets:** Nordic, Espressif, WCH, ARM, Atmel

---

Generated: 2025-04-29

All files ready for gist or GitHub publication.
