# Makefile — multi-platform barelog firmware
# Usage: make PLATFORM=atmega328p
#        make PLATFORM=ch32v203
#        make PLATFORM=rp2040

PLATFORM ?= atmega328p

# ================================================================
# Common settings
# ================================================================

BUILD_DIR := build_$(PLATFORM)

# Common sources (all platforms)
COMMON_SRCS := \
	config.c \
	sd.c \
	fat32_state.c \
	fat32_mount.c \
	fat32_log.c \
	fat32_vol.c \
	fat32_trim.c \
	log.c \
	repl.c \
	main.c

# Platform-specific sources and settings
include platform_$(PLATFORM).mk

# ================================================================
# Derived variables
# ================================================================

OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(COMMON_SRCS)) \
        $(patsubst %.c,$(BUILD_DIR)/%.o,$(PLATFORM_SRCS))

ELF := $(BUILD_DIR)/barelog.elf
HEX := $(BUILD_DIR)/barelog.hex
BIN := $(BUILD_DIR)/barelog.bin
MAP := $(BUILD_DIR)/barelog.map

# ================================================================
# Flags
# ================================================================

CFLAGS += -Wall -Wextra -std=c99 -Os -g
CFLAGS += -I.

LDFLAGS += -Wl,-Map=$(MAP)

# ================================================================
# Targets
# ================================================================

.PHONY: all size clean flash help

all: $(HEX)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(ELF): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

size: $(ELF)
	$(SIZE) -C $(ELF)

clean:
	rm -rf build_*

flash: $(HEX)
	$(FLASHER) $(FLASHER_FLAGS) -Uflash:w:$(HEX):i

help:
	@echo "barelog firmware"
	@echo ""
	@echo "Platforms: atmega328p ch32v203 rp2040"
	@echo ""
	@echo "make [PLATFORM=<name>]       Build"
	@echo "make PLATFORM=<name> size    Sizes"
	@echo "make PLATFORM=<name> flash   Flash"
	@echo "make clean                   Clean all"
	@echo ""
	@echo "Example: make PLATFORM=atmega328p flash PORT=/dev/ttyUSB0"
