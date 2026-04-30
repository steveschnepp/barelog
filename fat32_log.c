#include <stdint.h>
#include <string.h>
#include <avr/eeprom.h>
#include "fat32_priv.h"
#include "fat32.h"
#include "config.h"

/* ------------------------------------------------------------------ */
/* make_logname                                                        */
/* ------------------------------------------------------------------ */

/*
 * Fill name[11] with the 8.3 directory name for LOGnnnnn.TXT.
 * Example: num=1 -> "LOG00001TXT" (no dot, space-padded by format).
 * Uses a signed loop index to avoid uint8_t underflow on decrement.
 * num must be in [1..65534].
 */
static void make_logname(uint8_t name[11], uint16_t num)
{
	int8_t   i;
	uint16_t n = num;

	name[0] = 'L';
	name[1] = 'O';
	name[2] = 'G';

	/* fill digits 7..3 right-to-left (LSB first) */
	for (i = 7; i >= 3; i--) {
		name[i] = '0' + (uint8_t)(n % 10);
		n /= 10;
	}

	name[8]  = 'T';
	name[9]  = 'X';
	name[10] = 'T';
}

/* ------------------------------------------------------------------ */
/* scan_root_dir                                                       */
/* ------------------------------------------------------------------ */

/*
 * Scan the root directory cluster chain for name[11].
 *
 * Returns:
 *   1  entry found; *found_lba and *found_off point to it.
 *   0  not found; *empty_lba and *empty_off hold first available slot
 *      (0x00 end-of-directory or 0xE5 deleted entry).
 *  -1  SD error.
 *
 * Stops at first 0x00 name byte (end of directory).
 * Skips 0xE5 (deleted), volume labels (attr bit 3), subdirs (attr bit 4).
 */
static int8_t scan_root_dir(const uint8_t name[11],
                              uint32_t *found_lba,  uint16_t *found_off,
                              uint32_t *empty_lba,  uint16_t *empty_off)
{
	uint8_t  *buf = ring;
	uint32_t  clus = vol.root_clus;
	uint8_t   got_empty = 0;
	int8_t    ret;

	*found_lba = 0;
	*empty_lba = 0;

	while (clus >= 2 && clus < 0x0FFFFFF8UL) {
		uint32_t lba = clus_to_lba(clus);
		uint8_t  s;

		for (s = 0; s < vol.sec_per_clus; s++) {
			uint16_t e;

			ret = sd_read_sector(lba + s, buf);
			if (ret != SD_OK)
				return -1;

			for (e = 0; e < 512; e += 32) {
				struct dir_entry *d = (struct dir_entry *)(buf + e);
				uint8_t first = d->name[0];

				if (first == 0x00) {
					/* end-of-directory marker */
					if (!got_empty) {
						*empty_lba = lba + s;
						*empty_off = e;
					}
					goto done;
				}

				if (first == 0xE5) {
					/* deleted entry: usable slot */
					if (!got_empty) {
						*empty_lba = lba + s;
						*empty_off = e;
						got_empty  = 1;
					}
					continue;
				}

				if (d->attr & 0x08) continue; /* volume label */
				if (d->attr & 0x10) continue; /* subdirectory */

				{
					uint8_t j;
					uint8_t match = 1;

					for (j = 0; j < 11; j++) {
						if (d->name[j] != name[j]) {
							match = 0;
							break;
						}
					}
					if (match) {
						*found_lba = lba + s;
						*found_off = e;
						return 1;
					}
				}
			}
		}

		{
			uint32_t next;

			ret = fat_read_entry(clus, &next);
			if (ret != 0)
				return -1;
			clus = next; /* already masked to 28 bits */
		}
	}

done:
	return 0;
}

/* ------------------------------------------------------------------ */
/* fat32_open_log                                                      */
/* ------------------------------------------------------------------ */

int8_t fat32_open_log(struct config *cfg)
{
	uint8_t          name[11];
	uint32_t         found_lba, empty_lba;
	uint16_t         found_off, empty_off;
	uint32_t         prealloc_sz;
	uint32_t         n_sectors, n_clus;
	uint32_t         i;
	int8_t           ret;
	uint8_t         *buf = ring;
	struct dir_entry *de;

	prealloc_sz = config_prealloc_bytes(cfg->prealloc_code);
	n_sectors   = prealloc_sz / 512UL;
	n_clus      = (n_sectors + vol.sec_per_clus - 1) / vol.sec_per_clus;

	/*
	 * Find the next LOGnnnnn.TXT name not already in use.
	 * If the file exists and its file_size matches prealloc_sz, it is
	 * a resumable pre-allocated file from a previous interrupted session.
	 * If the file exists with a different size, skip to the next number.
	 */
	while (cfg->log_num < 65535U) {
		make_logname(name, cfg->log_num);
		ret = scan_root_dir(name,
		                    &found_lba, &found_off,
		                    &empty_lba, &empty_off);
		if (ret < 0)
			return ret;

		if (ret == 1) {
			ret = sd_read_sector(found_lba, buf);
			if (ret != SD_OK)
				return ret;

			de = (struct dir_entry *)(buf + found_off);
			if (de->file_size == prealloc_sz) {
				/*
				 * Resume: read bytes_written from EEPROM.
				 * Stored as two 16-bit halves (lo, hi) to
				 * allow atomic-enough single-byte writes.
				 */
				uint16_t lo, hi;
				uint32_t bw;
				uint8_t *p;

				p  = (uint8_t *)EEPROM_OFF_REC_LO;
				lo = (uint16_t)eeprom_read_byte(p)
				   | ((uint16_t)eeprom_read_byte(p + 1) << 8);

				p  = (uint8_t *)EEPROM_OFF_REC_HI;
				hi = (uint16_t)eeprom_read_byte(p)
				   | ((uint16_t)eeprom_read_byte(p + 1) << 8);

				bw = (uint32_t)lo | ((uint32_t)hi << 16);

				logfile.first_clus    = ((uint32_t)de->fst_clus_hi << 16)
				                      | de->fst_clus_lo;
				logfile.cur_lba       = clus_to_lba(logfile.first_clus)
				                      + bw / 512UL;
				logfile.end_lba       = clus_to_lba(logfile.first_clus)
				                      + n_sectors;
				logfile.dir_lba       = found_lba;
				logfile.dir_offset    = found_off;
				logfile.bytes_written = bw;
				logfile.prealloc_size = prealloc_sz;

				/*
				 * Walk the cluster chain to find last_clus.
				 * Cannot assume contiguity: original allocation
				 * may have been fragmented.
				 */
				{
					uint32_t c    = logfile.first_clus;
					uint32_t next = c;
					uint32_t entry;

					while (next < 0x0FFFFFF8UL) {
						c = next;
						ret = fat_read_entry(c, &entry);
						if (ret != 0)
							return ret;
						next = entry;
					}
					logfile.last_clus = c;
				}

				return 0;
			}

			cfg->log_num++;
			continue;
		}
		break; /* name is free */
	}

	if (empty_lba == 0)
		return -1; /* directory full */

	/*
	 * Allocate n_clus clusters, building the FAT chain single-pass.
	 * As each free cluster is found, the previous one's FAT entry is
	 * written to point at it. This avoids storing the full list.
	 */
	{
		uint32_t prev_clus  = 0;
		uint32_t first_clus = 0;
		uint32_t last_clus  = 0;
		uint32_t found_clus = 0;
		uint32_t c;
		uint32_t val;

		for (c = 2; found_clus < n_clus; c++) {
			ret = fat_read_entry(c, &val);
			if (ret != 0)
				return ret;
			if (val != 0)
				continue; /* cluster in use */

			if (found_clus == 0)
				first_clus = c;

			if (prev_clus != 0) {
				ret = fat_write_entry(prev_clus, c);
				if (ret != 0)
					return ret;
			}
			prev_clus = c;
			last_clus = c;
			found_clus++;
		}

		if (found_clus < n_clus)
			return -1; /* disk full */

		/* end-of-chain marker */
		ret = fat_write_entry(last_clus, 0x0FFFFFFFUL);
		if (ret != 0)
			return ret;

		logfile.first_clus    = first_clus;
		logfile.last_clus     = last_clus;
		logfile.cur_lba       = clus_to_lba(first_clus);
		logfile.end_lba       = logfile.cur_lba + n_sectors;
		logfile.dir_lba       = empty_lba;
		logfile.dir_offset    = empty_off;
		logfile.bytes_written = 0;
		logfile.prealloc_size = prealloc_sz;

		/* erase hint to card before zero-fill */
		sd_erase(logfile.cur_lba, logfile.end_lba - 1);

		/* zero-fill entire pre-allocated region */
		for (i = logfile.cur_lba; i < logfile.end_lba; i++) {
			ret = sd_write_zero_sector(i);
			if (ret != SD_OK)
				return ret;
		}

		/* write directory entry with file_size = prealloc_sz */
		ret = sd_read_sector(empty_lba, buf);
		if (ret != SD_OK)
			return ret;

		de = (struct dir_entry *)(buf + empty_off);
		memset(de, 0, 32);

		{
			uint8_t j;
			for (j = 0; j < 11; j++)
				de->name[j] = name[j];
		}

		de->attr        = 0x20; /* archive */
		de->fst_clus_hi = (uint16_t)(first_clus >> 16);
		de->fst_clus_lo = (uint16_t)(first_clus & 0xFFFF);
		de->file_size   = prealloc_sz;

		ret = sd_write_sector(empty_lba, buf);
		if (ret != SD_OK)
			return ret;
	}

	cfg->log_num++;
	config_save(cfg);

	return 0;
}

/* ------------------------------------------------------------------ */
/* fat32_append_sector                                                 */
/* ------------------------------------------------------------------ */

int8_t fat32_append_sector(const uint8_t *span_a, uint16_t len_a,
                            const uint8_t *span_b, uint16_t len_b)
{
	int8_t ret;

	/*
	 * Extend by one cluster when pre-allocation is exhausted.
	 * logfile.prealloc_size is not updated here; it reflects the
	 * original allocation and is used only for the resume check.
	 */
	if (logfile.cur_lba >= logfile.end_lba) {
		uint32_t c;
		uint32_t val;
		uint32_t lba, end, s;

		/* find one free cluster */
		for (c = 2; ; c++) {
			ret = fat_read_entry(c, &val);
			if (ret != 0)
				return ret;
			if (val == 0)
				break;
		}

		ret = fat_write_entry(logfile.last_clus, c);
		if (ret != 0)
			return ret;

		ret = fat_write_entry(c, 0x0FFFFFFFUL);
		if (ret != 0)
			return ret;

		lba = clus_to_lba(c);
		end = lba + vol.sec_per_clus;
		sd_erase(lba, end - 1);

		for (s = lba; s < end; s++) {
			ret = sd_write_zero_sector(s);
			if (ret != SD_OK)
				return ret;
		}

		logfile.end_lba   = end;
		logfile.last_clus = c;
	}

	ret = sd_write_sector_zc(logfile.cur_lba, span_a, len_a, span_b, len_b);
	if (ret != SD_OK)
		return ret;

	logfile.cur_lba++;
	logfile.bytes_written += (uint32_t)len_a + len_b;

	return 0;
}

/* ------------------------------------------------------------------ */
/* fat32_flush                                                         */
/* ------------------------------------------------------------------ */

int8_t fat32_flush(void)
{
	uint8_t          *buf = ring;
	int8_t            ret;
	struct dir_entry *de;

	/*
	 * Called either from the idle path (ISR running, available < 512)
	 * or from command mode (ISR off). Uses ring[0..511] as SD scratch.
	 * See fat32_priv.h for the full ring safety analysis.
	 * A future revision should add a tail >= 512 precondition check
	 * when called from the idle path to eliminate the unsafe window.
	 */
	ret = sd_read_sector(logfile.dir_lba, buf);
	if (ret != SD_OK)
		return ret;

	de = (struct dir_entry *)(buf + logfile.dir_offset);
	de->file_size = logfile.bytes_written;

	return sd_write_sector(logfile.dir_lba, buf);
}

/* ------------------------------------------------------------------ */
/* fat32_bytes_written                                                 */
/* ------------------------------------------------------------------ */

uint32_t fat32_bytes_written(void)
{
	return logfile.bytes_written;
}
