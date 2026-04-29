#include <stdint.h>
#include <avr/io.h>
#include "spi.h"

void spi_init(void)
{
	/* outputs: CS, MOSI, SCK */
	SPI_DDR |=  (1 << SPI_CS) | (1 << SPI_MOSI) | (1 << SPI_SCK);
	/* input: MISO (pull-up not needed; SD card drives it) */
	SPI_DDR &= ~(1 << SPI_MISO);

	/* idle CS and MOSI high; floating MOSI causes no harm in mode 0
	 * but a defined idle level avoids bus contention on shared lines */
	SPI_PORT |= (1 << SPI_CS) | (1 << SPI_MOSI);

	/* SPI enable, master mode, mode 0 (CPOL=0 CPHA=0), fosc/128
	 * SPR1=1 SPR0=1 gives /128; SPI2X=0 keeps it at /128 not /64 */
	SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR1) | (1 << SPR0);
	SPSR &= ~(1 << SPI2X);
}

void spi_set_fast(void)
{
	/* fosc/2: clear both prescaler bits in SPCR, set SPI2X in SPSR
	 * effective divisor = base(2) / SPI2X(2) = fosc/2 = 8 MHz */
	SPCR &= ~((1 << SPR1) | (1 << SPR0));
	SPSR |=  (1 << SPI2X);
}

void spi_write_buf(const uint8_t *buf, uint16_t len)
{
	uint16_t i;

	for (i = 0; i < len; i++)
		spi_transfer(buf[i]);
}

void spi_write_zeros(uint16_t len)
{
	uint16_t i;

	/* send actual 0x00 data bytes, not dummy bytes — caller is
	 * zero-filling a pre-allocated sector, not clocking the card */
	for (i = 0; i < len; i++)
		spi_transfer(0x00);
}

void spi_read_buf(uint8_t *buf, uint16_t len)
{
	uint16_t i;

	/* 0xFF is the SD SPI dummy output byte during card-to-host reads */
	for (i = 0; i < len; i++)
		buf[i] = spi_transfer(0xFF);
}
