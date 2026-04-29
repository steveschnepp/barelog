# platform_ch32v203.mk
# CH32V203C8T6 RISC-V with native USB CDC

CC := riscv64-unknown-elf-gcc
OBJCOPY := riscv64-unknown-elf-objcopy
SIZE := riscv64-unknown-elf-size
FLASHER := minichlink

MCU := ch32v203
F_CPU := 144000000UL

PLATFORM_SRCS := platform_ch32v203.c

CFLAGS += -march=rv32imafc -mabi=ilp32f
CFLAGS += -DF_CPU=$(F_CPU) -DUSE_USB_CDC
CFLAGS += -ffunction-sections -fdata-sections

LDFLAGS += -march=rv32imafc -mabi=ilp32f
LDFLAGS += -Wl,--gc-sections

# Build .bin for minichlink
all: $(BIN)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

FLASHER_FLAGS := -w $(BIN) 0x0

flash: $(BIN)
	$(FLASHER) $(FLASHER_FLAGS)
