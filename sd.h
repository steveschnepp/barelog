#ifndef SD_H
#define SD_H

#include <stdint.h>

/*
 * Return codes. All functions return SD_OK (0) on success or one of
 * the negative error codes below on failure.
 */
#define SD_OK           0
#define SD_ERR_TIMEOUT -1  /* card did not respond within the retry limit */
#define SD_ERR_INIT    -2  /* init sequence failed (card absent or unsupported) */
#define SD_ERR_WRITE   -4  /* card rejected a write or data response bad */
#define SD_ERR_READ    -5  /* CMD17 rejected or data token not received */

/*
 * Run the full SD SPI init sequence.
 *
 * Sequence: 80 dummy clocks, CMD0, CMD8, ACMD41 (HCS=1), CMD58, CMD16
 * (SDSC only), CMD59 (CRC off). Switches SPI to fosc/2 on success.
 *
 * Must be called with spi_init() already done (fosc/128 clock).
 * Returns SD_OK or negative error.
 */
int8_t sd_init(void);

/*
 * Read one 512-byte sector at LBA lba into buf.
 * buf must point to at least 512 bytes of writable memory.
 * Issues CMD17. Polls for data token 0xFE, then reads 512 bytes + 2 CRC.
 */
int8_t sd_read_sector(uint32_t lba, uint8_t *buf);

/*
 * Write one 512-byte sector at LBA lba from a single contiguous buffer.
 * buf may point into ring[]. Issues CMD24 with data token 0xFE.
 */
int8_t sd_write_sector(uint32_t lba, const uint8_t *buf);

/*
 * Write one 512-byte sector from two contiguous spans (zero-copy ring wrap).
 * span_a[0..len_a-1] is written first, then span_b[0..len_b-1].
 * len_a + len_b must equal 512.
 * span_b may be NULL only if len_b == 0.
 * Both spans may point into ring[].
 */
int8_t sd_write_sector_zc(uint32_t lba,
                           const uint8_t *span_a, uint16_t len_a,
                           const uint8_t *span_b, uint16_t len_b);

/*
 * Write one 512-byte sector of 0x00 at LBA lba without a source buffer.
 * Uses spi_write_zeros(). For pre-allocation zero-fill only.
 */
int8_t sd_write_zero_sector(uint32_t lba);

/*
 * Issue an erase command for LBA range [lba_start, lba_end] inclusive.
 * Sends CMD32 (erase start), CMD33 (erase end), CMD38 (execute).
 * Waits for the card to finish. Non-fatal: always returns SD_OK.
 * Cards are not required to honour erase; the call is best-effort.
 * Do not call while the UART ISR is running and ring data is live.
 */
int8_t sd_erase(uint32_t lba_start, uint32_t lba_end);

#endif /* SD_H */
