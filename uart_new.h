#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "platform.h"

/*
 * Ring buffer and transport interface.
 * Platform layer (uart_init, uart_putc, uart_puts_P, uart_getc_poll)
 * is defined per-platform in platform/atmega328p.c, ch32v203.c, etc.
 *
 * Caller references: ring_available(), ring_read(), ring_consume()
 * via the inline helpers in platform.h.
 */

/* compatibility aliases for existing code */
#define uart_available() ring_available()
#define uart_read()      ring_read()
#define uart_consume(n)  ring_consume(n)

#endif /* UART_H */
