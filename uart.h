#ifndef UART_H
#define UART_H

#include <stdint.h>

/*
 * Receive ring buffer.
 *
 * Size is 1024 bytes. At 115200 baud, one byte arrives every ~87 µs.
 * The ring holds ~89 ms of data at that rate, giving the main loop
 * ample time to drain it in 512-byte sector bursts.
 *
 * Concurrency contract:
 *   ring_head — written only by ISR(USART_RX_vect). Read by main loop.
 *   ring_tail — written only by main-loop context (log_process, log_flush).
 *               Read by ISR only implicitly via the full-check.
 *
 * ring[] is also used as scratch buffer in command mode and during
 * fat32 mount/open (when the ISR is either not yet enabled or is
 * disabled by repl_enter). See repl.c for the memory layout in that mode.
 */
#define RING_SIZE 1024
#define RING_MASK (RING_SIZE - 1)

extern uint8_t           ring[RING_SIZE];
extern volatile uint16_t ring_head;
extern volatile uint16_t ring_tail;

/*
 * Initialise USART0.
 * Enables U2X (double-speed) mode for better baud accuracy at 16 MHz.
 * Enables RX, TX, and RXCIE. Does not call sei().
 * Unknown baud values are silently replaced with 115200.
 */
void uart_init(uint32_t baud);

/*
 * Number of bytes currently in the ring.
 * Read of ring_head is not atomic on AVR (two 8-bit loads for a 16-bit
 * value). This is safe for the >= 512 threshold used in log_process:
 * a torn read can only undercount, never overcount, so we may defer a
 * sector write by one iteration at most.
 */
static inline uint16_t uart_available(void)
{
	return (ring_head - ring_tail) & RING_MASK;
}

/*
 * Read one byte from the ring and advance tail.
 * Caller must ensure uart_available() > 0.
 * Used by repl_getc path is direct UDR0 poll; this is for log_process
 * and any other main-loop consumer.
 */
static inline uint8_t uart_read(void)
{
	uint8_t b    = ring[ring_tail];
	ring_tail    = (ring_tail + 1) & RING_MASK;
	return b;
}

/*
 * Advance tail by n bytes without reading the data.
 * Used by log_process after a zero-copy sector write to SD:
 * the data was already sent to the card directly from ring[],
 * so we just move the tail forward.
 * Caller must ensure n <= uart_available().
 */
static inline void uart_consume(uint16_t n)
{
	ring_tail = (ring_tail + n) & RING_MASK;
}

/*
 * Blocking transmit of one byte via USART0.
 * Spins on UDRE0. Used for prompt characters and error output.
 */
void uart_putc(uint8_t c);

/*
 * Transmit a NUL-terminated string from program memory (PROGMEM).
 * Used for boot banner and REPL output to avoid placing string
 * literals in the 2 KB SRAM.
 */
void uart_puts_P(const char *s);

#endif /* UART_H */
