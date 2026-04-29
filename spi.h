#ifndef SPI_H
#define SPI_H

#include <stdint.h>
#include <avr/io.h>

/*
 * SPI pin assignment for ATmega328 hardware SPI (fixed by silicon):
 *
 *   PB2 = SS   — used as SD card CS (chip select), active low
 *   PB3 = MOSI — master out slave in
 *   PB4 = MISO — master in slave out
 *   PB5 = SCK  — clock
 *
 * PB2 must be configured as output for the SPI hardware to operate in
 * master mode, even though we drive it manually for CS timing control.
 *
 * The UART ISR touches PIND (PD5 LED toggle) and never PORTB, so
 * the read-modify-write on PORTB in spi_cs_assert/release is safe.
 */
#define SPI_DDR  DDRB
#define SPI_PORT PORTB
#define SPI_CS   PB2
#define SPI_MOSI PB3
#define SPI_MISO PB4
#define SPI_SCK  PB5

/*
 * Initialise SPI as master.
 * Clock is fosc/128 (~125 kHz at 16 MHz) for the SD card power-up
 * and init sequence, which must run below 400 kHz per the SD spec.
 * Call spi_set_fast() after sd_init() succeeds.
 */
void spi_init(void);

/*
 * Switch SPI clock to fosc/2 (8 MHz at 16 MHz).
 * SD cards in SPI mode support up to 25 MHz; 8 MHz is well within spec
 * and is the fastest the ATmega328 hardware SPI can produce.
 */
void spi_set_fast(void);

/*
 * Transfer one byte: send data, return received byte.
 * Blocks until SPIF is set (transfer complete).
 * Inlined because it is called in tight loops inside sd.c and is
 * the inner loop of every sector read and write.
 */
static inline uint8_t spi_transfer(uint8_t data)
{
	SPDR = data;
	while (!(SPSR & (1 << SPIF)))
		;
	return SPDR;
}

/*
 * Assert CS low (select SD card).
 * Must be followed by at least one spi_transfer before any command.
 */
static inline void spi_cs_assert(void)
{
	SPI_PORT &= ~(1 << SPI_CS);
}

/*
 * Deassert CS high (deselect SD card).
 * The SD card requires at least one dummy clock byte (0xFF) after CS
 * goes high before it will accept the next command. Callers in sd.c
 * send this explicitly.
 */
static inline void spi_cs_release(void)
{
	SPI_PORT |= (1 << SPI_CS);
}

/*
 * Write len bytes from buf to SPI. Received bytes are discarded.
 * buf may point into ring[] for zero-copy sector writes.
 */
void spi_write_buf(const uint8_t *buf, uint16_t len);

/*
 * Write len bytes of 0x00 to SPI without a source buffer.
 * Used exclusively for pre-allocation zero-fill of SD sectors.
 * 0x00 is the intended data content, not a dummy byte — SD dummy
 * bytes use 0xFF (see spi_read_buf).
 */
void spi_write_zeros(uint16_t len);

/*
 * Read len bytes from SPI into buf.
 * Sends 0xFF as the dummy output byte, which is the SD SPI convention
 * for "no data from host" during a card-to-host transfer.
 */
void spi_read_buf(uint8_t *buf, uint16_t len);

#endif /* SPI_H */
