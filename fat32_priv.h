#ifndef FAT32_PRIV_H
#define FAT32_PRIV_H

/*
 * Internal header for the fat32 subsystem.
 * Included only by fat32_state.c, fat32_mount.c, fat32_log.c,
 * fat32_vol.c, and fat32_trim.c. Never included by external code.
 *
 * Contains:
 *   - on-disk struct definitions
 *   - vol and logfile state declarations (defined in fat32_state.c)
 *   - inline cluster/FAT arithmetic helpers
 *   - declarations of fat_read_entry and fat_write_entry
 */

#include <stdint.h>
#include "uart.h"  /* for ring[] */
#include "sd.h"

/* ------------------------------------------------------------------ */
/* on-disk structures                                                  */
/* ------------------------------------------------------------------ */

/*
 * One entry in the MBR partition table (16 bytes).
 * Four entries live at MBR offsets 446, 462, 478, 494.
 * We use only type and lba_start; CHS fields are ignored.
 */
struct __attribute__((packed)) mbr_part {
	uint8_t  status;        /* 0x80 = bootable, 0x00 = not */
	uint8_t  chs_first[3]; /* CHS of first sector (ignored) */
	uint8_t  type;          /* 0x0B = FAT32/CHS, 0x0C = FAT32/LBA */
	uint8_t  chs_last[3];  /* CHS of last sector (ignored) */
	uint32_t lba_start;    /* LBA of first sector in partition */
	uint32_t lba_count;    /* number of sectors in partition */
};

/*
 * BIOS Parameter Block overlaid on the first 90 bytes of the VBR.
 * All unused fields are present only to keep struct offsets correct.
 */
struct __attribute__((packed)) bpb {
	uint8_t  jmp[3];
	uint8_t  oem[8];
	uint16_t bytes_per_sec;  /* must be 512 */
	uint8_t  sec_per_clus;   /* sectors per cluster (power of 2) */
	uint16_t rsvd_sec;       /* reserved sectors before first FAT */
	uint8_t  num_fats;       /* number of FAT copies (typically 2) */
	uint16_t root_ent_cnt;   /* 0 for FAT32 */
	uint16_t tot_sec16;      /* 0 for FAT32 */
	uint8_t  media;
	uint16_t fat_sz16;       /* 0 for FAT32; non-zero means FAT12/16 */
	uint16_t sec_per_trk;
	uint16_t num_heads;
	uint32_t hidd_sec;
	uint32_t tot_sec32;      /* total sectors in volume */
	uint32_t fat_sz32;       /* sectors per FAT copy */
	uint16_t ext_flags;
	uint16_t fs_ver;
	uint32_t root_clus;      /* cluster number of root directory */
	uint16_t fs_info;
	uint16_t bk_boot_sec;
	uint8_t  reserved[12];
	uint8_t  drv_num;
	uint8_t  reserved1;
	uint8_t  boot_sig;
	uint32_t vol_id;
	uint8_t  vol_lab[11];
	uint8_t  fs_type[8];
};

/*
 * 32-byte FAT32 directory entry.
 * We write name, attr, cluster hi/lo, and file_size only.
 * All timestamp fields are left as zero.
 */
struct __attribute__((packed)) dir_entry {
	uint8_t  name[11];       /* 8.3, space-padded, no dot, uppercase */
	uint8_t  attr;           /* 0x20 = archive (normal file) */
	uint8_t  nt_res;
	uint8_t  crt_time_tenth;
	uint16_t crt_time;
	uint16_t crt_date;
	uint16_t lst_acc_date;
	uint16_t fst_clus_hi;    /* high 16 bits of first cluster number */
	uint16_t wrt_time;
	uint16_t wrt_date;
	uint16_t fst_clus_lo;    /* low 16 bits of first cluster number */
	uint32_t file_size;      /* current file size in bytes */
};

/* ------------------------------------------------------------------ */
/* volume state (defined in fat32_state.c)                            */
/* ------------------------------------------------------------------ */

/*
 * Populated once by fat32_mount(). Treated as read-only by all other
 * functions. Describes the layout of the mounted FAT32 partition.
 */
struct fat32_vol {
	uint32_t fat_lba;      /* LBA of FAT copy 0 first sector */
	uint32_t fat_sz32;     /* sectors per FAT copy */
	uint32_t data_lba;     /* LBA of data region (cluster 2) */
	uint32_t root_clus;    /* cluster number of root directory */
	uint32_t part_lba;     /* LBA of partition VBR */
	uint32_t tot_sec;      /* total sectors in partition */
	uint8_t  sec_per_clus; /* sectors per cluster */
	uint8_t  num_fats;     /* number of FAT copies */
};

extern struct fat32_vol vol;

/* ------------------------------------------------------------------ */
/* log file state (defined in fat32_state.c)                          */
/* ------------------------------------------------------------------ */

/*
 * Populated by fat32_open_log(). Tracks the current write position
 * and metadata for the open log file. Updated by fat32_append_sector
 * and fat32_flush.
 */
struct fat32_logfile {
	uint32_t first_clus;    /* first cluster of the log file */
	uint32_t cur_lba;       /* next sector LBA to write */
	uint32_t end_lba;       /* one past last allocated sector LBA */
	uint32_t dir_lba;       /* sector containing the directory entry */
	uint16_t dir_offset;    /* byte offset of dir entry in dir_lba */
	uint32_t bytes_written; /* total bytes written since file open */
	uint32_t prealloc_size; /* allocation size at open (resume check) */
	uint32_t last_clus;     /* last cluster in chain (for extension) */
};

extern struct fat32_logfile logfile;

/* ------------------------------------------------------------------ */
/* inline cluster and FAT arithmetic                                   */
/* ------------------------------------------------------------------ */

/*
 * LBA of the first sector of a cluster.
 * Cluster numbers start at 2; cluster 2 is at data_lba.
 */
static inline uint32_t clus_to_lba(uint32_t clus)
{
	return vol.data_lba + (uint32_t)(clus - 2) * vol.sec_per_clus;
}

/*
 * FAT sector number containing the entry for cluster clus.
 * 128 entries per 512-byte sector (4 bytes each).
 */
static inline uint32_t fat_sector(uint32_t clus)
{
	return vol.fat_lba + clus / 128U;
}

/*
 * Byte offset within a FAT sector for cluster clus's entry.
 */
static inline uint16_t fat_offset(uint32_t clus)
{
	return (uint16_t)((clus % 128U) * 4U);
}

/* ------------------------------------------------------------------ */
/* FAT entry I/O (defined in fat32_state.c)                           */
/* ------------------------------------------------------------------ */

/*
 * Read the 28-bit FAT32 entry for cluster clus into *val.
 * Uses ring[0..511] as sector scratch.
 */
int8_t fat_read_entry(uint32_t clus, uint32_t *val);

/*
 * Write val into the FAT entry for cluster clus.
 * Preserves the upper 4 reserved bits. Writes all FAT copies.
 * Uses ring[0..511] as sector scratch.
 */
int8_t fat_write_entry(uint32_t clus, uint32_t val);

#endif /* FAT32_PRIV_H */
