# platform_esp32s3.mk
# Espressif ESP32-S3 (Single Xtensa core, 240 MHz, USB device)

CC := xtensa-esp32s3-elf-gcc
OBJCOPY := xtensa-esp32s3-elf-objcopy
SIZE := xtensa-esp32s3-elf-size
FLASHER := esptool.py

MCU := esp32s3
F_CPU := 240000000UL

PLATFORM_SRCS := platform_esp32s3.c

# ESP32-S3 specific flags
CFLAGS += -DF_CPU=$(F_CPU) -DUSE_USB_CDC
CFLAGS += -mlongcalls -mtext-section-literals
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -nostdlib -Wl,--entry=call_start_cpu0

LDFLAGS += -mlongcalls -mtext-section-literals
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -Wl,-static

# Partition table and bootloader (precompiled)
BOOTLOADER := bootloader_esp32s3.bin
PARTITION_TABLE := partition_table_esp32s3.bin

# Binary output for esptool.py
all: $(BIN)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

# esptool.py handles bootloader + partition table + app
FLASHER_FLAGS := --chip esp32s3 --port /dev/ttyUSB0 \
                 --baud 460800 write_flash \
                 0x0 $(BOOTLOADER) \
                 0x8000 $(PARTITION_TABLE) \
                 0x20000 $(BIN)

flash: $(BIN)
	$(FLASHER) $(FLASHER_FLAGS)

PORT ?= /dev/ttyUSB0
