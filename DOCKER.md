# Docker — barelog Build Environment

Dockerfiles for all five platforms. Optimized for Debian 13 (trixie).

## Images and sizes

| Image | Dockerfile | Size | Platforms | Use case |
|-------|-----------|------|-----------|----------|
| barelog:latest | Dockerfile_optimized | ~2.5 GB | All 5 | Complete dev |
| barelog:avr | Dockerfile.atmega328p | ~500 MB | ATmega328P only | Minimal AVR |
| barelog:arm | Dockerfile.arm | ~700 MB | RP2040, nRF52840 | ARM only |
| barelog:riscv | Dockerfile.riscv | ~600 MB | CH32V203 | RISC-V only |
| barelog:esp32s3 | Dockerfile.esp32s3 | ~1.2 GB | ESP32-S3 | Xtensa only |

## Quick start

### Build complete image (all platforms)

```sh
docker build -f Dockerfile_optimized -t barelog:latest .
```

Or with docker-compose:

```sh
docker-compose build barelog
docker-compose run --rm barelog bash
```

### Build specific platform

```sh
# ATmega328P
docker build -f Dockerfile.atmega328p -t barelog:avr .

# RP2040 / nRF52840
docker build -f Dockerfile.arm -t barelog:arm .

# CH32V203
docker build -f Dockerfile.riscv -t barelog:riscv .

# ESP32-S3
docker build -f Dockerfile.esp32s3 -t barelog:esp32s3 .
```

## Usage

### Run complete environment

```sh
# Interactive bash
docker run -it --rm -v $(pwd):/workspace barelog:latest bash

# Build single platform
docker run -it --rm -v $(pwd):/workspace barelog:latest \
  make PLATFORM=atmega328p

# Build all platforms
docker run -it --rm -v $(pwd):/workspace barelog:latest \
  bash -c "make clean && \
           make PLATFORM=atmega328p && \
           make PLATFORM=ch32v203 && \
           make PLATFORM=rp2040 && \
           make PLATFORM=nrf52840 && \
           make PLATFORM=esp32s3"
```

### With docker-compose

```sh
# All platforms
docker-compose run --rm barelog bash
docker-compose run --rm barelog make PLATFORM=atmega328p

# Specific platform
docker-compose run --rm barelog-avr make
docker-compose run --rm barelog-arm make PLATFORM=rp2040
docker-compose run --rm barelog-riscv make
docker-compose run --rm barelog-esp32s3 make
```

## Toolchains included

| Toolchain | Image(s) | Package(s) |
|-----------|----------|-----------|
| AVR GCC | barelog:* | gcc-avr, avr-libc, avrdude |
| ARM EABI | barelog:*, barelog:arm | arm-none-eabi-gcc, arm-none-eabi-gdb, libnewlib-arm-none-eabi |
| RISC-V | barelog:*, barelog:riscv | riscv64-unknown-elf-gcc, riscv64-unknown-elf-gdb |
| Xtensa (ESP32-S3) | barelog:*, barelog:esp32s3 | xtensa-esp32s3-elf (precompiled from Espressif) |
| Build tools | all | build-essential, git, make, cmake, python3, vim, nano |

## Build strategy

**Dockerfile_optimized** uses multi-stage builds to minimize final image size:

1. **Stage 1 (builder):** Base Debian 13 + build tools
2. **Stage 2 (with-arm):** + ARM toolchain
3. **Stage 3 (with-riscv):** + RISC-V toolchain
4. **Stage 4 (with-avr):** + AVR toolchain
5. **Stage 5 (with-xtensa):** + Xtensa toolchain + esptool.py
6. **Final:** Copy all binaries from stage 5, keep final clean

Layers are reused across build: if you rebuild with only ARM changes, only stage 2-5 rebuild.

## Examples

### Build and show sizes

```sh
docker run -it --rm -v $(pwd):/workspace barelog:latest \
  bash -c "make PLATFORM=atmega328p size"
```

### Build and flash ATmega328P

```sh
docker run -it --rm \
  -v $(pwd):/workspace \
  -v /dev/ttyUSB0:/dev/ttyUSB0 \
  --device /dev/ttyUSB0 \
  barelog:avr \
  bash -c "make && make flash PORT=/dev/ttyUSB0"
```

### Build all platforms in parallel

```sh
docker run -it --rm -v $(pwd):/workspace barelog:latest \
  bash -c "
    make PLATFORM=atmega328p &
    make PLATFORM=ch32v203 &
    make PLATFORM=rp2040 &
    make PLATFORM=nrf52840 &
    make PLATFORM=esp32s3 &
    wait
  "
```

## Layer reuse

Building multiple images reuses layers:

```sh
# First build (full)
docker build -f Dockerfile_optimized -t barelog:latest .  # ~2.5 GB

# Second build (reuses stages)
docker build -f Dockerfile.esp32s3 -t barelog:esp32s3 .  # ~300 MB more (new layer)

# Build specific: reuses base from main image if exists
docker build -f Dockerfile.arm -t barelog:arm .  # ~100 MB more (partial reuse)
```

## Network requirements

Xtensa toolchain requires downloading from Espressif GitHub (if building esp32s3 image):
- ~200 MB download
- Requires internet access during build

Other toolchains come from Debian repos (cached).

## Storage notes

**For all images:**
```sh
# Total storage across all images
docker images | grep barelog

# Cleanup unused layers
docker system prune -a

# Remove specific image
docker rmi barelog:latest
```

## Tips

1. **Build once, use many times:**
   ```sh
   docker build -f Dockerfile_optimized -t barelog:latest .
   # Now use multiple times without rebuild
   ```

2. **Cache optimization:** Don't change apt install order; Docker caches layers.

3. **Fast iteration:** Mount source as volume, edit locally, build in container.

4. **CI/CD:** These Dockerfiles work in GitHub Actions, GitLab CI, etc.

5. **Arm64 hosts:** Some toolchains may not have arm64 binaries; x86_64 Intel/AMD only.
