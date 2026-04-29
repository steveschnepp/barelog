#include <stdint.h>
#include <string.h>
#include "eeprom_flash.h"

/* ================================================================
 * Flash-based EEPROM
 * 
 * Strategy:
 *   - 256 bytes of EEPROM data cached in RAM
 *   - Lazy write: changes buffered in cache
 *   - Wear leveling: sync() writes to first free location in sector
 *   - When sector full: erase and start over
 * 
 * Flash layout (4 KB sector):
 *   [0..255]      EEPROM copy (generation 0)
 *   [256]         generation counter (0xFF = unused, 0xFE = gen 0, etc)
 *   [257..511]    padding/unused
 *   [512..767]    EEPROM copy (generation 1)
 *   [768]         generation counter
 *   [769..1023]   padding/unused
 *   [1024..4095]  spare space for more generations if needed
 * 
 * This gives ~16 writes per sector (at 256 bytes + 1 counter per generation)
 * before full erase. Enough for most use cases.
 * ================================================================ */

#define EEPROM_SIZE 256
#define EEPROM_GEN_OFFSET (EEPROM_SIZE)
#define EEPROM_SLOT_SIZE (EEPROM_GEN_OFFSET + 1)
#define EEPROM_NUM_SLOTS (EEPROM_FLASH_SIZE / EEPROM_SLOT_SIZE)

static uint8_t eeprom_cache[EEPROM_SIZE];
static uint8_t eeprom_dirty = 0;

/* ================================================================
 * Find the current generation slot (most recent write)
 * ================================================================ */

static uint16_t find_current_gen(void)
{
	uint16_t slot;
	uint8_t  gen_byte = 0xFF;
	uint16_t current_slot = 0;

	for (slot = 0; slot < EEPROM_NUM_SLOTS; slot++) {
		uint32_t addr = EEPROM_FLASH_BASE_ADDR + slot * EEPROM_SLOT_SIZE + EEPROM_GEN_OFFSET;
		uint8_t  g;

		eeprom_flash_read_bytes(addr, &g, 1);

		/* 0xFF = unused, 0xFE = gen 0, 0xFD = gen 1, etc
		 * higher byte value = older generation */
		if (g != 0xFF && g < gen_byte) {
			gen_byte = g;
			current_slot = slot;
		}
	}

	return current_slot;
}

/* ================================================================
 * Find next free slot
 * ================================================================ */

static uint16_t find_next_free_slot(uint16_t current_slot)
{
	uint16_t next = (current_slot + 1) % EEPROM_NUM_SLOTS;

	if (next == current_slot) {
		/* sector full, need to erase */
		eeprom_flash_erase_sector(EEPROM_FLASH_BASE_ADDR);
		return 0;
	}

	return next;
}

/* ================================================================
 * Public API
 * ================================================================ */

void eeprom_flash_init(void)
{
	uint16_t slot = find_current_gen();
	uint32_t addr = EEPROM_FLASH_BASE_ADDR + slot * EEPROM_SLOT_SIZE;

	eeprom_flash_read_bytes(addr, eeprom_cache, EEPROM_SIZE);
	eeprom_dirty = 0;
}

uint8_t eeprom_read_byte(uint16_t addr)
{
	if (addr >= EEPROM_SIZE)
		return 0xFF;
	return eeprom_cache[addr];
}

void eeprom_write_byte(uint16_t addr, uint8_t val)
{
	if (addr >= EEPROM_SIZE)
		return;
	eeprom_cache[addr] = val;
	eeprom_dirty = 1;
}

void eeprom_update_byte(uint16_t addr, uint8_t val)
{
	if (addr >= EEPROM_SIZE)
		return;
	if (eeprom_cache[addr] != val) {
		eeprom_cache[addr] = val;
		eeprom_dirty = 1;
	}
}

void eeprom_flash_sync(void)
{
	uint16_t current_slot, next_slot;
	uint32_t next_addr;
	uint8_t  gen_byte;

	if (!eeprom_dirty)
		return;

	current_slot = find_current_gen();
	next_slot = find_next_free_slot(current_slot);
	next_addr = EEPROM_FLASH_BASE_ADDR + next_slot * EEPROM_SLOT_SIZE;

	/* Write EEPROM data */
	eeprom_flash_write_bytes(next_addr, eeprom_cache, EEPROM_SIZE);

	/* Write generation byte: one less than current (older = higher number)
	 * If current is 0xFE, next is 0xFD; if none exist, use 0xFE */
	{
		uint8_t current_gen;
		uint32_t current_addr = EEPROM_FLASH_BASE_ADDR + current_slot * EEPROM_SLOT_SIZE + EEPROM_GEN_OFFSET;

		eeprom_flash_read_bytes(current_addr, &current_gen, 1);
		gen_byte = (current_gen == 0xFF) ? 0xFE : current_gen - 1;
	}

	eeprom_flash_write_bytes(next_addr + EEPROM_GEN_OFFSET, &gen_byte, 1);

	eeprom_dirty = 0;
}
