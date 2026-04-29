#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* ring buffer — shared across all platforms                          */
/* ------------------------------------------------------------------ */

#define RING_SIZE 1024
#define RING_MASK (RING_SIZE - 1)

extern uint8_t           ring[RING_SIZE];
extern volatile uint16_t ring_head;
extern volatile uint16_t ring_tail;

static inline uint16_t ring_available(void)
{
	return (ring_head - ring_tail) & RING_MASK;
}

static inline uint8_t ring_read(void)
{
	uint8_t b = ring[ring_tail];
	ring_tail = (ring_tail + 1) & RING_MASK;
	return b;
}

static inline void ring_consume(uint16_t n)
{
	ring_tail = (ring_tail + n) & RING_MASK;
}

/* ------------------------------------------------------------------ */
/* UART/USB CDC transport — platform picks at compile time            */
/* ------------------------------------------------------------------ */

void uart_init(uint32_t baud);
void uart_putc(uint8_t c);
void uart_puts_P(const char *s);
uint8_t uart_getc_poll(void);

/* ------------------------------------------------------------------ */
/* SPI — block transfer I/O                                           */
/* ------------------------------------------------------------------ */

void spi_init(void);
void spi_set_fast(void);

uint8_t spi_transfer(uint8_t data);
void spi_cs_assert(void);
void spi_cs_release(void);

void spi_write_buf(const uint8_t *buf, uint16_t len);
void spi_write_zeros(uint16_t len);
void spi_read_buf(uint8_t *buf, uint16_t len);

/* ------------------------------------------------------------------ */
/* Timer — 500 ms CTC for idle flush                                  */
/* ------------------------------------------------------------------ */

void timer_init(void);
uint8_t timer_flush_pending(void);
void timer_clear_pending(void);
void timer_restart(void);

/* ------------------------------------------------------------------ */
/* EEPROM — byte-level access                                         */
/* ------------------------------------------------------------------ */

void eeprom_init(void);
uint8_t eeprom_read_byte(uint16_t addr);
void eeprom_write_byte(uint16_t addr, uint8_t val);
void eeprom_update_byte(uint16_t addr, uint8_t val);

/* ------------------------------------------------------------------ */
/* GPIO — STAT LED                                                    */
/* ------------------------------------------------------------------ */

void gpio_led_init(void);
void gpio_led_toggle(void);

/* ------------------------------------------------------------------ */
/* platform_init — call once at startup                               */
/* ------------------------------------------------------------------ */

void platform_init(void);

/* ------------------------------------------------------------------ */
/* platform-specific inlines (SPI register access)                    */
/* ------------------------------------------------------------------ */

#if defined(__AVR_ATmega328P__)
  #include <avr/io.h>

  #define SPI_DDR  DDRB
  #define SPI_PORT PORTB
  #define SPI_CS   PB2
  #define SPI_MOSI PB3
  #define SPI_MISO PB4
  #define SPI_SCK  PB5

  static inline uint8_t spi_transfer(uint8_t data)
  {
	SPDR = data;
	while (!(SPSR & (1 << SPIF)))
		;
	return SPDR;
  }

  static inline void spi_cs_assert(void)
  {
	SPI_PORT &= ~(1 << SPI_CS);
  }

  static inline void spi_cs_release(void)
  {
	SPI_PORT |= (1 << SPI_CS);
  }

#elif defined(__riscv) || defined(__RISCV__)
  /* CH32V203 — TODO: implement register defs */
  extern uint8_t spi_transfer(uint8_t data);
  extern void spi_cs_assert(void);
  extern void spi_cs_release(void);

#elif defined(__arm__) || defined(__ARM_ARCH_6M__)
  /* RP2040 — TODO: implement register defs */
  extern uint8_t spi_transfer(uint8_t data);
  extern void spi_cs_assert(void);
  extern void spi_cs_release(void);

#else
  #error "Unknown platform"
#endif

#endif /* PLATFORM_H */
