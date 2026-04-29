#ifndef LOG_H
#define LOG_H

#include <stdint.h>

/*
 * Drain ring data to SD in 512-byte zero-copy sector writes.
 *
 * Called from the main loop continuously. For each full 512-byte chunk
 * available in the ring, builds two spans covering the data (handling
 * the ring wrap), scans for the escape sequence, then writes the sector
 * directly to SD without copying. Resets the Timer1 idle counter after
 * each sector write. Returns when fewer than 512 bytes remain in the ring.
 *
 * If the escape sequence is detected, calls repl_enter() which never
 * returns (watchdog reboot on exit from command mode).
 */
void log_process(void);

/*
 * Flush the partial sector and persist state.
 *
 * Copies any remaining ring bytes (< 512) into ring[0..511] as a
 * zero-padded sector, appends it to the log file, then calls
 * fat32_flush() to update the directory entry. Finally writes
 * bytes_written to EEPROM (recovery record) using eeprom_update_byte
 * to avoid unnecessary wear on unchanged values.
 *
 * Called when flush_pending is set and uart_available() < 512.
 * Also called by repl_enter() before entering command mode.
 *
 * Return values from fat32_append_sector and fat32_flush are
 * intentionally ignored: a mid-session SD error is unrecoverable
 * without a power cycle, and silent continuation preserves partial data.
 */
void log_flush(void);

/*
 * Verify the stack canary placed at __bss_end by main().
 * Calls error_halt(ERR_STACK) if the canary value has been overwritten.
 * Called from the main loop after each idle flush.
 */
void log_check_canary(void);

/*
 * Return non-zero if Timer1 COMPA ISR has fired since last cleared.
 * Used by main() to decide when to call log_flush().
 */
uint8_t log_flush_pending(void);

/*
 * Clear the flush_pending flag. Called by main() before log_flush()
 * so that a new timer tick during the flush schedules the next one.
 */
void log_clear_flush_pending(void);

#endif /* LOG_H */
