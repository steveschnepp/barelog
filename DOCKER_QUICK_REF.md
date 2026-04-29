# Docker Quick Reference

## Build images

```sh
# All platforms (recommended)
docker build -f Dockerfile_optimized -t barelog:latest .

# Single platform
docker build -f Dockerfile.atmega328p -t barelog:avr .
docker build -f Dockerfile.arm -t barelog:arm .
docker build -f Dockerfile.riscv -t barelog:riscv .
docker build -f Dockerfile.esp32s3 -t barelog:esp32s3 .

# With docker-compose
docker-compose build
docker-compose build barelog          # all platforms
docker-compose build barelog-avr      # ATmega328P
docker-compose build barelog-arm      # ARM
docker-compose build barelog-riscv    # RISC-V
docker-compose build barelog-esp32s3  # ESP32-S3
```

## Run builds

```sh
# Interactive shell (all platforms)
docker run -it --rm -v $(pwd):/workspace barelog:latest bash

# Build specific platform
docker run -it --rm -v $(pwd):/workspace barelog:latest \
  make PLATFORM=rp2040

# Show sizes
docker run -it --rm -v $(pwd):/workspace barelog:latest \
  make PLATFORM=ch32v203 size

# Clean
docker run -it --rm -v $(pwd):/workspace barelog:latest make clean
```

## With docker-compose

```sh
# Enter all-platforms container
docker-compose run --rm barelog bash

# Build specific platform
docker-compose run --rm barelog make PLATFORM=nrf52840

# Run command and exit
docker-compose run --rm barelog make PLATFORM=atmega328p size

# Run on specific service
docker-compose run --rm barelog-arm make
docker-compose run --rm barelog-riscv make PLATFORM=ch32v203
```

## Build all platforms

```sh
# Using provided script
./build_all.sh                    # sequential
PARALLEL=4 ./build_all.sh         # 4 parallel jobs

# Manual
docker run -it --rm -v $(pwd):/workspace barelog:latest bash -c \
  "make clean && \
   for p in atmega328p ch32v203 rp2040 nrf52840 esp32s3; do \
     make PLATFORM=\$p; \
   done"
```

## Flash via Docker

```sh
# ATmega328P to /dev/ttyUSB0
docker run -it --rm \
  -v $(pwd):/workspace \
  --device /dev/ttyUSB0 \
  barelog:avr \
  make flash PORT=/dev/ttyUSB0

# RP2040 with openocd
docker run -it --rm \
  -v $(pwd):/workspace \
  --device /dev/bus/usb \
  barelog:arm \
  make PLATFORM=rp2040 flash

# ESP32-S3 with esptool
docker run -it --rm \
  -v $(pwd):/workspace \
  --device /dev/ttyUSB0 \
  barelog:esp32s3 \
  make flash PORT=/dev/ttyUSB0
```

## Manage images

```sh
# List images
docker images | grep barelog

# Show image size
docker images --human-readable barelog

# Remove image
docker rmi barelog:latest
docker rmi barelog:avr

# Inspect image
docker inspect barelog:latest

# Check layers
docker history barelog:latest
```

## Cache management

```sh
# Prune unused layers
docker system prune

# Prune everything (use with caution)
docker system prune -a

# Force rebuild (ignore cache)
docker build --no-cache -f Dockerfile_optimized -t barelog:latest .
```

## Troubleshooting

```sh
# Check if image exists
docker image inspect barelog:latest 2>/dev/null && echo "exists" || echo "not found"

# See full build output
docker build -f Dockerfile_optimized -t barelog:latest . --progress=plain

# Run with detailed output
docker run -it --rm -v $(pwd):/workspace barelog:latest \
  make PLATFORM=rp2040 -d

# Check toolchain
docker run -it --rm barelog:latest which arm-none-eabi-gcc
docker run -it --rm barelog:latest riscv64-unknown-elf-gcc --version
docker run -it --rm barelog:latest avr-gcc --version
docker run -it --rm barelog:esp32s3 xtensa-esp32s3-elf-gcc --version
```

## Common workflows

### Development cycle

```sh
# Start container once
docker-compose run barelog bash

# Inside container, iterate:
make PLATFORM=rp2040
make PLATFORM=rp2040 size
make clean
make PLATFORM=atmega328p
# ... etc
```

### CI/CD (local simulation)

```sh
# Simulate GitHub Actions
docker run -it --rm \
  -v $(pwd):/workspace \
  -e CI=true \
  barelog:latest \
  bash -c "
    for p in atmega328p ch32v203 rp2040 nrf52840 esp32s3; do
      echo \"Building \$p...\"
      make PLATFORM=\$p || exit 1
    done
  "
```

### Parallel builds

```sh
docker-compose run barelog bash -c \
  "make PLATFORM=atmega328p & \
   make PLATFORM=ch32v203 & \
   make PLATFORM=rp2040 & \
   make PLATFORM=nrf52840 & \
   make PLATFORM=esp32s3 & \
   wait"
```

## Image sizes (approximate)

| Image | Size | Time to build |
|-------|------|---------------|
| barelog:avr | 500 MB | 1 min |
| barelog:arm | 700 MB | 1 min |
| barelog:riscv | 600 MB | 1 min |
| barelog:esp32s3 | 1.2 GB | 3 min |
| barelog:latest | 2.5 GB | 5 min |
