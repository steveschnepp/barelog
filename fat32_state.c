#include "fat32_priv.h"

/*
 * Global state definitions for the fat32 subsystem.
 *
 * Both structs are non-static so they can be shared across the
 * fat32_mount.c, fat32_log.c, fat32_vol.c, and fat32_trim.c
 * translation units via the extern declarations in fat32_priv.h.
 *
 * No other code outside the fat32 subsystem accesses these directly;
 * external callers use the public API in fat32.h.
 */

struct fat32_vol     vol;
struct fat32_logfile logfile;

/* ------------------------------------------------------------------ */
/* FAT entry I/O — placed here because they depend only on vol and    */
/* ring, making fat32_state.c the natural home.                       */
/* ------------------------------------------------------------------ */

int8_t fat_read_entry(uint32_t clus, uint32_t *val)
{
	uint8_t  *buf = ring;
	uint32_t  sec = fat_sector(clus);
	uint16_t  off = fat_offset(clus);
	int8_t    ret;

	ret = sd_read_sector(sec, buf);
	if (ret != SD_OK)
		return ret;

	/* little-endian u32; mask upper reserved nibble per FAT32 spec */
	*val = ((uint32_t)buf[off]
	      | ((uint32_t)buf[off + 1] << 8)
	      | ((uint32_t)buf[off + 2] << 16)
	      | ((uint32_t)buf[off + 3] << 24)) & 0x0FFFFFFFUL;
	return 0;
}

int8_t fat_write_entry(uint32_t clus, uint32_t val)
{
	uint8_t  *buf = ring;
	uint32_t  sec = fat_sector(clus);
	uint16_t  off = fat_offset(clus);
	int8_t    ret;
	uint8_t   f;

	for (f = 0; f < vol.num_fats; f++) {
		uint32_t fsec = sec + (uint32_t)f * vol.fat_sz32;

		ret = sd_read_sector(fsec, buf);
		if (ret != SD_OK)
			return ret;

		/* preserve upper nibble (reserved bits) of the existing entry */
		buf[off]     = (uint8_t)(val);
		buf[off + 1] = (uint8_t)(val >> 8);
		buf[off + 2] = (uint8_t)(val >> 16);
		buf[off + 3] = (buf[off + 3] & 0xF0) | (uint8_t)(val >> 24);

		ret = sd_write_sector(fsec, buf);
		if (ret != SD_OK)
			return ret;
	}
	return 0;
}
