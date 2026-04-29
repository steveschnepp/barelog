# platform_nrf52840.mk
# Nordic nRF52840 (Cortex-M4, 64 MHz, USB device)

CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size
FLASHER := nrfjprog

MCU := nrf52840
F_CPU := 64000000UL

PLATFORM_SRCS := platform_nrf52840.c

CFLAGS += -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
CFLAGS += -DF_CPU=$(F_CPU) -DUSE_USB_CDC
CFLAGS += -ffunction-sections -fdata-sections

LDFLAGS += -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
LDFLAGS += -Wl,--gc-sections

# Binary output for nrfjprog
all: $(HEX)

FLASHER_FLAGS := --program $(HEX) --chiperase --reset

flash: $(HEX)
	$(FLASHER) $(FLASHER_FLAGS)
