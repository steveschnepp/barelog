#include <stdint.h>
#include <stddef.h>
#include <avr/io.h>
#include "sd.h"
#include "spi.h"
#include "util.h"

static struct {
	uint8_t sdhc; /* 1 = SDHC/SDXC (block-addressed), 0 = SDSC (byte-addressed) */
} sd_state;

/*
 * Send one SD SPI command frame and read the R1 response byte.
 *
 * SD SPI command frame: 6 bytes.
 *   byte 0 : 0x40 | cmd  (start bit 0, transmission bit 1, cmd index)
 *   byte 1 : arg[31:24]
 *   byte 2 : arg[23:16]
 *   byte 3 : arg[15:8]
 *   byte 4 : arg[7:0]
 *   byte 5 : crc (7-bit CRC7 + stop bit 1; CRC is only checked for
 *             CMD0 and CMD8 — all others accept 0x01 as a dummy)
 *
 * CS must already be asserted by the caller before calling sd_cmd.
 * sd_cmd does not assert or release CS.
 *
 * The card responds after 0 to 8 dummy bytes (Ncr). We poll up to 8
 * bytes. If no non-0xFF byte appears, *r1 is set to 0xFF and we return
 * SD_ERR_TIMEOUT.
 */
static int8_t sd_cmd(uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t *r1)
{
	uint8_t i;
	uint8_t resp;

	spi_transfer(0x40 | cmd);
	spi_transfer((uint8_t)(arg >> 24));
	spi_transfer((uint8_t)(arg >> 16));
	spi_transfer((uint8_t)(arg >>  8));
	spi_transfer((uint8_t)(arg      ));
	spi_transfer(crc);

	/* poll up to 8 bytes for a valid (non-0xFF) R1 response */
	for (i = 0; i < 8; i++) {
		resp = spi_transfer(0xFF);
		if (resp != 0xFF) {
			*r1 = resp;
			return SD_OK;
		}
	}

	*r1 = 0xFF;
	return SD_ERR_TIMEOUT;
}

/*
 * Send an application-specific command (ACMD).
 * ACMDs are prefixed by CMD55 (APP_CMD) which tells the card the next
 * command is an ACMD. The gap byte between CMD55 and the ACMD gives
 * the card time to process CMD55 before the next frame starts.
 * CS remains asserted throughout; caller must assert before calling.
 */
static int8_t sd_acmd(uint8_t cmd, uint32_t arg, uint8_t *r1)
{
	int8_t ret;

	ret = sd_cmd(55, 0, 0x01, r1);
	if (ret != SD_OK)
		return ret;
	spi_transfer(0xFF); /* Ncr gap between CMD55 and ACMD */
	return sd_cmd(cmd, arg, 0x01, r1);
}

/*
 * Wait for the card to release the MISO line (busy signal).
 * While busy, the card holds MISO low (0x00). When ready, MISO
 * returns to 0xFF. Each call to spi_transfer at 8 MHz takes ~1 µs,
 * so 50000 iterations gives a ~50 ms timeout.
 * Only called after spi_set_fast(), so the 8 MHz rate applies.
 */
static int8_t sd_wait_ready(void)
{
	uint16_t i;

	for (i = 0; i < 50000U; i++) {
		if (spi_transfer(0xFF) == 0xFF)
			return SD_OK;
	}
	return SD_ERR_TIMEOUT;
}

int8_t sd_init(void)
{
	uint8_t  i;
	uint8_t  r1 = 0xFF; /* initialised: safe if sd_cmd never succeeds */
	uint8_t  r7[4];
	uint16_t tries;
	int8_t   ret;

	/*
	 * Power-up sequence: CS high, 80 dummy clocks (10 bytes × 8 bits).
	 * The SD spec requires at least 74 clocks with CS and MOSI high
	 * before sending the first command.
	 */
	spi_cs_release();
	for (i = 0; i < 10; i++)
		spi_transfer(0xFF);

	/*
	 * CMD0 (GO_IDLE_STATE): reset the card into SPI mode.
	 * CRC 0x95 is the correct CRC7 for CMD0 with arg 0x00.
	 * Retry up to 10 times; some cards need several attempts.
	 * Expected R1 response: 0x01 (in idle state).
	 */
	spi_cs_assert();
	for (i = 0; i < 10; i++) {
		ret = sd_cmd(0, 0, 0x95, &r1);
		if (ret == SD_OK && r1 == 0x01)
			break;
		spi_transfer(0xFF); /* gap between retries */
	}
	if (r1 != 0x01) {
		spi_cs_release();
		return SD_ERR_INIT;
	}
	spi_transfer(0xFF);

	/*
	 * CMD8 (SEND_IF_COND): check host voltage range and card version.
	 * Arg 0x000001AA: voltage range 2.7-3.6 V (0x01), check pattern 0xAA.
	 * CRC 0x87 is the correct CRC7 for this exact CMD8 argument.
	 * R7 response is 5 bytes: R1 + 4 bytes OCR echo.
	 * If the card does not respond to CMD8, it is SDv1/SDSC; continue.
	 * If it responds, verify the echo pattern to confirm compatibility.
	 */
	ret = sd_cmd(8, 0x000001AAUL, 0x87, &r1);
	if (ret == SD_OK && r1 == 0x01) {
		for (i = 0; i < 4; i++)
			r7[i] = spi_transfer(0xFF);
		/* r7[3] = check pattern echo, r7[2] bits[3:0] = voltage range */
		if (r7[3] != 0xAA || (r7[2] & 0x0F) != 0x01) {
			spi_cs_release();
			return SD_ERR_INIT;
		}
	}
	/* CMD8 unsupported: treat as SDv1, proceed without version check */
	spi_transfer(0xFF);

	/*
	 * ACMD41 (SD_SEND_OP_COND): start card initialisation.
	 * HCS=1 (bit 30) signals that we support SDHC/SDXC block addressing.
	 * Loop until R1 == 0x00 (card left idle state = init complete).
	 * Retry up to 1000 times with 1 ms delay; some cards take >100 ms.
	 */
	for (tries = 0; tries < 1000; tries++) {
		ret = sd_acmd(41, 0x40000000UL, &r1);
		spi_transfer(0xFF);
		if (ret == SD_OK && r1 == 0x00)
			break;
		delay_ms(1);
	}
	if (r1 != 0x00) {
		spi_cs_release();
		return SD_ERR_INIT;
	}

	/*
	 * CMD58 (READ_OCR): read the Operating Conditions Register.
	 * CCS bit (bit 30, OCR byte 0 bit 6) indicates card capacity type:
	 *   CCS=1: SDHC/SDXC — addresses are block numbers (512-byte blocks)
	 *   CCS=0: SDSC      — addresses are byte offsets
	 */
	ret = sd_cmd(58, 0, 0x01, &r1);
	if (ret != SD_OK || r1 != 0x00) {
		spi_cs_release();
		return SD_ERR_INIT;
	}
	{
		uint8_t ocr[4];
		for (i = 0; i < 4; i++)
			ocr[i] = spi_transfer(0xFF);
		sd_state.sdhc = (ocr[0] & 0x40) ? 1 : 0;
	}
	spi_transfer(0xFF);

	/*
	 * CMD16 (SET_BLOCKLEN): force 512-byte blocks for SDSC cards.
	 * SDHC/SDXC always use 512-byte blocks; CMD16 is not needed.
	 */
	if (!sd_state.sdhc) {
		ret = sd_cmd(16, 512, 0x01, &r1);
		if (ret != SD_OK || r1 != 0x00) {
			spi_cs_release();
			return SD_ERR_INIT;
		}
		spi_transfer(0xFF);
	}

	/*
	 * CMD59 (CRC_ON_OFF): disable CRC checking.
	 * CRC is optional in SPI mode and costs throughput; we disable it.
	 * The return value is not checked — CMD59 is advisory.
	 */
	sd_cmd(59, 0, 0x01, &r1);
	spi_transfer(0xFF);

	spi_cs_release();
	spi_set_fast(); /* safe to go to fosc/2 now that init is done */

	return SD_OK;
}

/*
 * Convert an LBA sector number to the argument value for SD commands.
 * SDHC/SDXC use block addresses (lba directly).
 * SDSC uses byte addresses (lba << 9, i.e. lba * 512).
 */
static uint32_t sd_lba_arg(uint32_t lba)
{
	if (sd_state.sdhc)
		return lba;
	return lba << 9;
}

int8_t sd_read_sector(uint32_t lba, uint8_t *buf)
{
	uint8_t  r1;
	uint8_t  tok;
	uint16_t i;
	int8_t   ret;

	spi_cs_assert();

	/*
	 * CMD17 (READ_SINGLE_BLOCK): read one 512-byte block.
	 * R1 must be 0x00 (no error, not idle).
	 */
	ret = sd_cmd(17, sd_lba_arg(lba), 0x01, &r1);
	if (ret != SD_OK || r1 != 0x00) {
		spi_cs_release();
		return SD_ERR_READ;
	}

	/*
	 * Poll for the data token 0xFE. The card sends 0xFF while preparing
	 * the data, then 0xFE immediately before the 512 data bytes.
	 * Error tokens have bit 0 set and bit 7 clear; we do not decode them.
	 * 200 × ~1 µs = ~200 µs maximum wait.
	 */
	for (i = 0; i < 200; i++) {
		tok = spi_transfer(0xFF);
		if (tok == 0xFE)
			break;
	}
	if (tok != 0xFE) {
		spi_cs_release();
		return SD_ERR_TIMEOUT;
	}

	spi_read_buf(buf, 512);

	/* read and discard the 2 CRC bytes (CRC is disabled, values ignored) */
	spi_transfer(0xFF);
	spi_transfer(0xFF);

	spi_cs_release();
	spi_transfer(0xFF); /* mandatory post-CS dummy byte */

	return SD_OK;
}

/*
 * Common write completion: send CRC, read data response, wait for busy.
 * Called by all three write functions after the data payload is sent.
 * CS is asserted on entry and released on return (success or error).
 *
 * Data response token format: xxx0sss1
 *   sss = 010: data accepted
 *   sss = 101: CRC error (should not occur, CRC disabled)
 *   sss = 110: write error
 * We check bits [4:0] == 0b00101 (0x05) for accepted.
 */
static int8_t sd_write_finish(void)
{
	uint8_t resp;
	int8_t  ret;

	/* CRC bytes: dummy values, CRC checking is disabled */
	spi_transfer(0xFF);
	spi_transfer(0xFF);

	/* read data response token */
	resp = spi_transfer(0xFF);
	if ((resp & 0x1F) != 0x05) {
		spi_cs_release();
		return SD_ERR_WRITE;
	}

	/* wait for the card to finish writing internally (MISO goes high) */
	ret = sd_wait_ready();
	spi_cs_release();
	spi_transfer(0xFF); /* mandatory post-CS dummy byte */

	return ret; /* SD_OK or SD_ERR_TIMEOUT */
}

int8_t sd_write_sector(uint32_t lba, const uint8_t *buf)
{
	uint8_t r1;
	int8_t  ret;

	spi_cs_assert();

	/* CMD24 (WRITE_BLOCK): write one 512-byte block */
	ret = sd_cmd(24, sd_lba_arg(lba), 0x01, &r1);
	if (ret != SD_OK || r1 != 0x00) {
		spi_cs_release();
		return SD_ERR_WRITE;
	}

	spi_transfer(0xFF);  /* one Nwr gap byte before data token */
	spi_transfer(0xFE);  /* data token: single-block write */
	spi_write_buf(buf, 512);

	return sd_write_finish();
}

int8_t sd_write_sector_zc(uint32_t lba,
                           const uint8_t *span_a, uint16_t len_a,
                           const uint8_t *span_b, uint16_t len_b)
{
	uint8_t r1;
	int8_t  ret;

	spi_cs_assert();

	ret = sd_cmd(24, sd_lba_arg(lba), 0x01, &r1);
	if (ret != SD_OK || r1 != 0x00) {
		spi_cs_release();
		return SD_ERR_WRITE;
	}

	spi_transfer(0xFF);  /* Nwr gap */
	spi_transfer(0xFE);  /* data token */
	spi_write_buf(span_a, len_a);
	/* span_b is NULL when the ring data fits in one contiguous span */
	if (len_b > 0 && span_b != NULL)
		spi_write_buf(span_b, len_b);

	return sd_write_finish();
}

int8_t sd_write_zero_sector(uint32_t lba)
{
	uint8_t r1;
	int8_t  ret;

	spi_cs_assert();

	ret = sd_cmd(24, sd_lba_arg(lba), 0x01, &r1);
	if (ret != SD_OK || r1 != 0x00) {
		spi_cs_release();
		return SD_ERR_WRITE;
	}

	spi_transfer(0xFF);   /* Nwr gap */
	spi_transfer(0xFE);   /* data token */
	spi_write_zeros(512); /* 0x00 is the intended data, not a dummy */

	return sd_write_finish();
}

int8_t sd_erase(uint32_t lba_start, uint32_t lba_end)
{
	uint8_t r1;

	spi_cs_assert();

	/*
	 * Three-command erase sequence per SD spec section 7.2.7:
	 *   CMD32: set erase start address
	 *   CMD33: set erase end address
	 *   CMD38: execute erase
	 *
	 * Return values are intentionally ignored. The SD spec does not
	 * require cards to implement erase; many silently accept and
	 * ignore it. We treat the whole operation as best-effort.
	 */
	sd_cmd(32, sd_lba_arg(lba_start), 0x01, &r1);
	spi_transfer(0xFF);

	sd_cmd(33, sd_lba_arg(lba_end), 0x01, &r1);
	spi_transfer(0xFF);

	sd_cmd(38, 0, 0x01, &r1);

	/* erase can take hundreds of milliseconds on large ranges */
	sd_wait_ready();

	spi_cs_release();
	spi_transfer(0xFF);

	return SD_OK;
}
