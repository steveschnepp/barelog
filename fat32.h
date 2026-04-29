#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include "config.h"

/*
 * Public API for the FAT32 subsystem.
 * Implementation is split across:
 *   fat32_state.c  — vol/logfile state, FAT entry I/O
 *   fat32_mount.c  — fat32_mount
 *   fat32_log.c    — fat32_open_log, fat32_append_sector, fat32_flush
 *   fat32_vol.c    — volume layout getters
 *   fat32_trim.c   — TRIM operations
 * Internal types and helpers are in fat32_priv.h (not for external use).
 */

/*
 * Read MBR and VBR, populate internal volume state.
 * Must be called before any other fat32 function.
 * Uses ring[0..511] as scratch. UART ISR must be off.
 * Returns 0 on success, negative on error.
 */
int8_t fat32_mount(void);

/*
 * Find or resume a LOGnnnnn.TXT log file.
 * If a matching pre-allocated file exists, resumes it using the
 * EEPROM recovery offset. Otherwise creates and pre-allocates a new
 * file, erases and zero-fills it, writes the directory entry.
 * Updates cfg->log_num and saves to EEPROM on new file creation.
 * Uses ring[0..511] as scratch. UART ISR must be off.
 * Returns 0 on success, negative on error.
 */
int8_t fat32_open_log(struct config *cfg);

/*
 * Write one 512-byte sector to the log file from two ring spans.
 * span_b may be NULL if len_b == 0 (no ring wrap).
 * len_a + len_b must equal 512.
 * Extends the file by one cluster if pre-allocation is exhausted.
 * Returns 0 on success, negative on error.
 */
int8_t fat32_append_sector(const uint8_t *span_a, uint16_t len_a,
                            const uint8_t *span_b, uint16_t len_b);

/*
 * Update the directory entry file_size field to bytes_written.
 * Uses ring[0..511] as scratch.
 * Safe to call from command mode (ISR off) or idle path (available < 512).
 * Returns 0 on success, negative on error.
 */
int8_t fat32_flush(void);

/*
 * Return the number of bytes written to the log file since open.
 * Used by log_flush() to save the recovery record to EEPROM.
 */
uint32_t fat32_bytes_written(void);

/*
 * Volume layout getters. Used by repl.c for disk and trim commands.
 * Values are valid after fat32_mount() succeeds.
 */
uint32_t fat32_vol_data_lba(void);
uint32_t fat32_vol_fat_lba(void);
uint32_t fat32_vol_fat_sz32(void);
uint32_t fat32_vol_part_lba(void);
uint32_t fat32_vol_tot_sec(void);
uint8_t  fat32_vol_num_fats(void);
uint8_t  fat32_vol_sec_per_clus(void);

/*
 * Erase all free (unallocated) clusters on the volume.
 * Scans FAT, coalesces contiguous free runs, issues sd_erase per run.
 * Call from command mode only (ISR off, ring is full scratch).
 * Returns 0 on success, negative on SD read error.
 */
int8_t fat32_trim_free(void);

/*
 * Erase all data sectors (data_lba to end of partition).
 * Preserves MBR, VBR, and FAT. All file content is destroyed.
 * Call from command mode only.
 * Returns 0 always (sd_erase is non-fatal).
 */
int8_t fat32_trim_full(void);

/*
 * Erase entire card from LBA 0 to end of partition.
 * Destroys MBR, VBR, FAT, and all data. Card must be reformatted.
 * Call from command mode only.
 * Returns 0 always.
 */
int8_t fat32_trim_fuller(void);

#endif /* FAT32_H */
