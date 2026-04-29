#ifndef SPI_H
#define SPI_H

#include <stdint.h>
#include "platform.h"

/*
 * SPI master interface. All functions defined in platform layer.
 * spi_transfer() inlined in platform.h for speed.
 * spi_cs_assert/release() inlined in platform.h.
 */

/* init runs at fosc/128, spi_set_fast switches to fosc/2 after sd_init */
void spi_init(void);
void spi_set_fast(void);

/* bulk transfer */
void spi_write_buf(const uint8_t *buf, uint16_t len);
void spi_write_zeros(uint16_t len);
void spi_read_buf(uint8_t *buf, uint16_t len);

#endif /* SPI_H */
