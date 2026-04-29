# ESP32-S3 Single-Core Implementation

Using one CPU core (core 0) simplifies ESP32-S3 significantly.

## Why single-core?

**ESP32-S3 is dual-core by design, but barelog needs only one:**

- No multicore tasks (data logging is inherently sequential)
- No inter-core interrupts or communication
- No race conditions between cores
- Simpler RTOS (no RTOS at all, just ISRs)
- Faster startup (skip core 1 init)
- Lower power (one core idle instead of two)

**Core 1 disabled:** ROM bootloader can keep it off, or we disable in `platform_init()`.

## Implementation vs dual-core

### What stays the same
- USB CDC (single endpoint server)
- SPI (shared hardware, no multicore access)
- Timer (TIMG0 core 0 only)
- Flash erase/write (protected by ISR disable)
- Main loop (single-threaded)

### What gets simpler
- **ISRs:** No spinlocks, no atomic reads, no core synchronization
- **Interrupts:** NVIC handles one core, standard ARM behavior
- **Memory:** Only core 0 stack needed (~8 KB); core 1 stack unused
- **Wakeup:** No cross-core IPI, just CPU wakes on interrupt
- **Init sequence:** Half the clock setup, no app CPU startup

## Complexity ranking (revised)

With single-core constraint:

1. **nRF52840** — Standard ARM, clean NVMC (still easiest)
2. **ESP32-S3 (single-core)** — Still Xtensa, but simplified ISRs, no multicore (2nd easiest)
3. **RP2040** — ARM but SSI/flash SPI protocol (3rd)
4. **CH32V203** — RISC-V, smaller ecosystem (4th)

**ESP32-S3 single-core becomes competitive with RP2040.** Main complexity is USB CDC and Xtensa ISA, not multicore.

## Single-core setup in platform_esp32s3.c

```c
void platform_init(void)
{
	/* Disable core 1 (optional, may already be off)
	 * Set PRO_CPU_RESET_REL in DPORT_APPCPU_CTRL_REG
	 * or just don't start it in ROM bootloader config
	 */

	/* Core 0 clock: usually 240 MHz from ROM bootloader
	 * If needed, enable PLL:
	 * RTCCNTL.options0 set BIAS_I2C_FORCE_PU to 1
	 * RTCCNTL.clk_conf set SOC_CLK_SEL to PLL (2)
	 * RTCCNTL.dpll_con0/con1 set PLL dividers
	 */
}
```

## Timer ISR (no multicore)

```c
void TIMG0_T0_LEVEL_IRQHandler(void)
{
	/* Core 0 only, standard ARM NVIC behavior */
	if (TIMG0.int_st_timers.val & TIMG_T0_INT_ST) {
		TIMG0.int_clr_timers.t0_int = 1;
		timer_pending = 1;
	}
}
```

No need for:
- Spinlocks
- Cross-core wakeup
- Atomic operations beyond ISR disable
- Core affinity

## USB CDC ISR (no multicore)

```c
void USB_INTR_Handler(void)
{
	/* Core 0, standard USB device controller behavior */
	if (USB_SERIAL_JTAG.ep1_conf.wr_done) {
		/* Handle OUT endpoint
		 * Fill ring[] from USB FIFO
		 * No atomic read needed, ISR is single-core
		 */
		uint8_t byte = USB_SERIAL_JTAG.ep1.r_fifo;
		uint16_t next = (ring_head + 1) & RING_MASK;
		if (next != ring_tail) {
			ring[ring_head] = byte;
			ring_head = next;
		}
	}
}
```

## Main loop (single-threaded)

```c
int main(void)
{
	platform_init();  /* Disable core 1, clock setup */
	uart_init();      /* USB CDC on core 0 */
	spi_init();       /* SPI2 on core 0 */
	timer_init();     /* TIMG0 timer 0 on core 0 */

	sei();  /* Enable interrupts on core 0 */

	for (;;) {
		log_process();  /* Drain ring to SD */

		if (timer_pending && ring_available() < 512) {
			log_flush();  /* Flush partial sector */
			/* No spinlock needed, ISR won't run during flush */
		}

		/* Power saving (optional): sleep until next interrupt */
		/* esp_sleep_light() or just idle */
	}
}
```

No:
- Task switching
- Task yielding
- Semaphores
- Mutexes

## Advantages

| Aspect | Benefit |
|--------|---------|
| Startup | Faster (no core 1 CPU init) |
| Code | Simpler (no multicore primitives) |
| ISRs | Simpler (no cross-core sync) |
| Memory | Save ~8 KB (core 1 stack) |
| Power | Lower (one core idle) |
| Debugging | Easier (single execution context) |

## Disadvantages

None for barelog. Single-core is perfect fit.

(If you needed parallel processing later, enable core 1, but for a logger: overkill.)

## Implementation checklist

Same as other platforms:

1. GPIO LED (verify toolchain)
2. UART/USB CDC (debug output)
3. SPI init + transfer (SD)
4. Timer + ISR (500 ms)
5. Flash EEPROM (recovery)
6. Test boot: `1` `2` `<`
7. Test logging + escape sequence
8. Test command mode

No extra multicore steps.

## Revised platform complexity

**With single-core ESP32-S3:**

| Platform | Complexity | Reason |
|----------|-----------|--------|
| nRF52840 | Low | Standard ARM, clean SDK |
| ESP32-S3 | Low | Single-core, simple ISRs, but Xtensa ISA |
| RP2040 | Medium | ARM but SSI/flash protocol |
| CH32V203 | Medium | RISC-V, smaller ecosystem |

**Choose between nRF52840 and ESP32-S3:**
- nRF52840: More power-efficient, standard ARM
- ESP32-S3: 3x faster (240 vs 64 MHz), more RAM, still simple single-core

Both are ~equal effort now.

## References

- Espressif ESP32-S3 TRM (Technical Reference Manual)
- ESP-IDF (but we use bare register code, not IDF)
- Single-core setup: DPORT registers, clock dividers, ISR handlers
