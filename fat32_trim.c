#include "fat32_priv.h"
#include "fat32.h"

/*
 * TRIM operations: hint the SD card to erase sectors it no longer needs.
 * All three functions call sd_erase(), which is non-fatal and best-effort.
 * Must be called from command mode only (ISR off, ring is full scratch).
 */

/* ------------------------------------------------------------------ */
/* fat32_trim_free                                                     */
/* ------------------------------------------------------------------ */

/*
 * Erase all free (unallocated) clusters on the volume.
 *
 * Strategy: scan every FAT sector into ring[0..511]. For each cluster
 * with entry == 0 (free), compute its LBA range and extend or flush the
 * current contiguous run. Coalescing free clusters into runs reduces the
 * number of sd_erase() calls, which matters because each erase command
 * carries SD protocol overhead.
 *
 * Cluster index starts at 0 so the FAT sector loop is aligned to the
 * sector boundary from the first sector. Clusters 0 and 1 are reserved
 * and protected by an explicit guard.
 */
int8_t fat32_trim_free(void)
{
	uint8_t  *buf = ring;
	uint32_t  fat_sec;
	uint32_t  fat_end       = vol.fat_lba + vol.fat_sz32;
	uint32_t  run_start_lba = 0;
	uint32_t  run_end_lba   = 0;
	uint8_t   in_run        = 0;
	uint32_t  clus          = 0;
	int8_t    ret;

	for (fat_sec = vol.fat_lba; fat_sec < fat_end; fat_sec++) {
		uint16_t off;

		ret = sd_read_sector(fat_sec, buf);
		if (ret != SD_OK)
			return ret;

		for (off = 0; off < 512; off += 4, clus++) {
			uint32_t entry;

			if (clus < 2) {
				/* reserved clusters: break any open run */
				if (in_run) {
					sd_erase(run_start_lba, run_end_lba);
					in_run = 0;
				}
				continue;
			}

			entry = ((uint32_t)buf[off]
			       | ((uint32_t)buf[off + 1] << 8)
			       | ((uint32_t)buf[off + 2] << 16)
			       | ((uint32_t)buf[off + 3] << 24)) & 0x0FFFFFFFUL;

			if (entry == 0) {
				uint32_t lba_s = clus_to_lba(clus);
				uint32_t lba_e = lba_s + vol.sec_per_clus - 1;

				if (!in_run) {
					run_start_lba = lba_s;
					run_end_lba   = lba_e;
					in_run        = 1;
				} else if (lba_s == run_end_lba + 1) {
					run_end_lba = lba_e; /* extend */
				} else {
					sd_erase(run_start_lba, run_end_lba);
					run_start_lba = lba_s;
					run_end_lba   = lba_e;
				}
			} else {
				if (in_run) {
					sd_erase(run_start_lba, run_end_lba);
					in_run = 0;
				}
			}
		}
	}

	if (in_run)
		sd_erase(run_start_lba, run_end_lba);

	return 0;
}

/* ------------------------------------------------------------------ */
/* fat32_trim_full                                                     */
/* ------------------------------------------------------------------ */

/*
 * Erase all data sectors on the volume (data_lba to end of partition).
 * MBR, VBR, and FAT regions are preserved.
 * The volume structure survives but all file content is destroyed.
 */
int8_t fat32_trim_full(void)
{
	uint32_t end_lba = vol.part_lba + vol.tot_sec - 1;

	sd_erase(vol.data_lba, end_lba);
	return 0;
}

/* ------------------------------------------------------------------ */
/* fat32_trim_fuller                                                   */
/* ------------------------------------------------------------------ */

/*
 * Erase the entire card from LBA 0 to end of partition.
 * MBR, VBR, FAT, and all data are destroyed.
 * Card must be reformatted before use.
 */
int8_t fat32_trim_fuller(void)
{
	uint32_t end_lba = vol.part_lba + vol.tot_sec - 1;

	sd_erase(0, end_lba);
	return 0;
}
