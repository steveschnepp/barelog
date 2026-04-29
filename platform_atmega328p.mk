# platform_atmega328p.mk
# ATmega328P with Optiboot

CC := avr-gcc
OBJCOPY := avr-objcopy
SIZE := avr-size
FLASHER := avrdude

MCU := atmega328p
F_CPU := 16000000UL

PLATFORM_SRCS := platform_atmega328p.c

CFLAGS += -mmcu=$(MCU) -DF_CPU=$(F_CPU)
CFLAGS += -ffunction-sections -fdata-sections

LDFLAGS += -mmcu=$(MCU)
LDFLAGS += -Wl,--gc-sections

FLASHER_FLAGS := -p$(MCU) -cstk500v1 -b115200 -P$(PORT)

PORT ?= /dev/ttyUSB0
