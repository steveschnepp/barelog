#include "fat32_priv.h"

/*
 * fat32_mount — read MBR and VBR, populate the vol struct.
 *
 * Reads sector 0 (MBR), finds the first FAT32 partition (type 0x0B
 * or 0x0C), then reads its VBR to extract the BPB fields we need.
 * Uses ring[0..511] as sector scratch. ISR must be off or ring safe.
 */
int8_t fat32_mount(void)
{
	uint8_t         *buf = ring;
	struct mbr_part *part;
	struct bpb      *b;
	uint32_t         part_lba;
	uint8_t          i;
	int8_t           ret;

	ret = sd_read_sector(0, buf);
	if (ret != SD_OK)
		return ret;

	/*
	 * Scan four MBR partition table entries at byte offset 446.
	 * Type 0x0B = FAT32 with CHS addressing.
	 * Type 0x0C = FAT32 with LBA addressing.
	 * We accept both; we always use LBA ourselves.
	 */
	part_lba = 0;
	for (i = 0; i < 4; i++) {
		part = (struct mbr_part *)(buf + 446 + i * 16);
		if (part->type == 0x0B || part->type == 0x0C) {
			part_lba = part->lba_start;
			break;
		}
	}
	if (part_lba == 0)
		return -1;

	/* read Volume Boot Record at partition start */
	ret = sd_read_sector(part_lba, buf);
	if (ret != SD_OK)
		return ret;

	b = (struct bpb *)buf;

	/* reject non-512-byte-sector volumes (we assume 512 throughout) */
	if (b->bytes_per_sec != 512)
		return -1;

	/* fat_sz16 != 0 means FAT12 or FAT16, not FAT32 */
	if (b->fat_sz16 != 0 || b->fat_sz32 == 0)
		return -1;

	vol.part_lba     = part_lba;
	vol.fat_lba      = part_lba + b->rsvd_sec;
	vol.fat_sz32     = b->fat_sz32;
	vol.num_fats     = b->num_fats;
	vol.data_lba     = vol.fat_lba + (uint32_t)b->num_fats * b->fat_sz32;
	vol.root_clus    = b->root_clus;
	vol.sec_per_clus = b->sec_per_clus;
	vol.tot_sec      = b->tot_sec32;

	return 0;
}
