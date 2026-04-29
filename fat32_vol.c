#include "fat32_priv.h"
#include "fat32.h"

/*
 * Volume layout getters.
 * Expose selected vol fields to repl.c for the disk and trim commands.
 * External code must not access vol directly.
 */

uint32_t fat32_vol_data_lba(void)     { return vol.data_lba; }
uint32_t fat32_vol_fat_lba(void)      { return vol.fat_lba; }
uint32_t fat32_vol_fat_sz32(void)     { return vol.fat_sz32; }
uint32_t fat32_vol_part_lba(void)     { return vol.part_lba; }
uint32_t fat32_vol_tot_sec(void)      { return vol.tot_sec; }
uint8_t  fat32_vol_num_fats(void)     { return vol.num_fats; }
uint8_t  fat32_vol_sec_per_clus(void) { return vol.sec_per_clus; }
