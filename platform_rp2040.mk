# platform_rp2040.mk
# Raspberry Pi Pico with native USB CDC

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size
FLASHER := openocd

MCU := rp2040
F_CPU := 125000000UL

PLATFORM_SRCS := platform_rp2040.c

CFLAGS += -mcpu=cortex-m0plus -mthumb
CFLAGS += -DF_CPU=$(F_CPU) -DUSE_USB_CDC
CFLAGS += -ffunction-sections -fdata-sections

LDFLAGS += -mcpu=cortex-m0plus -mthumb
LDFLAGS += -Wl,--gc-sections

all: $(BIN)

$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

FLASHER_FLAGS := -f interface/cmsis-dap.cfg -f target/rp2040.cfg \
                 -c "adapter speed 5000" \
                 -c "program $(ELF) verify reset exit"

flash: $(ELF)
	$(FLASHER) $(FLASHER_FLAGS)
