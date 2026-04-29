# BareLog Firmware (OpenLog reimplementation)

Clean-room C99 reimplementation of the SparkFun OpenLog firmware for the
ATmega328P. No Arduino runtime, no external libraries, no dynamic allocation.

## Hardware

| Item | Value |
|---|---|
| MCU | ATmega328P |
| Bootloader | Optiboot |
| F_CPU | 16 MHz |
| Storage | microSD via SPI (FAT32, up to 64 GB) |
| UART | USART0, default 115200 8N1 |
| LED | STAT1 on PD5 |
| SD CS | PB2 |

## Boot sequence

On power-up the firmware emits three characters on UART before logging begins:

```
1   UART alive
2   SD init and FAT32 mount succeeded
<   log file open, logging mode active
```

If any init step fails, `<` is never sent and the STAT LED blinks an error
code repeatedly (see error codes below).

## Logging

Incoming UART bytes are stored in a 1024-byte ring buffer by the RX ISR.
The main loop drains the ring to SD in 512-byte zero-copy sector writes.
Each log file is pre-allocated and zero-filled at open time to avoid FAT
chain extension during writes.

Log files are named `LOGnnnnn.TXT` starting from `LOG00001.TXT`. The number
is stored in EEPROM and incremented on each new file.

### Pre-allocation sizes

| Code | Size |
|---|---|
| 0 | 1 MB |
| 1 | 4 MB |
| 2 | 8 MB (default) |
| 3 | 16 MB |
| 4 | 32 MB |

If the log grows beyond the pre-allocated size, the firmware extends the
file by one cluster at a time.

### Idle flush

A Timer1 interrupt fires every 500 ms. When it fires and fewer than 512
bytes are pending, the partial sector is zero-padded, written to SD, and the
directory entry is updated. The recovery offset is saved to EEPROM so that a
power loss mid-session can be resumed.

## Command mode

Send the escape character (`CTRL+Z`, ASCII 26) three consecutive times to
enter command mode. The prompt character `>` is emitted.

The UART RX interrupt is disabled in command mode. Bytes are read by direct
hardware poll. The full 1024-byte ring buffer is available as scratch.

### Commands

| Command | Action |
|---|---|
| `?` | print command list |
| `disk` | print volume layout (LBAs, sizes) |
| `init` | re-run SD init and FAT32 mount |
| `sync` | flush partial sector and update directory entry |
| `reset` | reboot via watchdog (returns to logging mode) |
| `trim free` | erase all unallocated clusters |
| `trim full` | erase all data sectors, preserve FAT/MBR |
| `trim fuller` | erase entire card from LBA 0 |

`reset` is the only way to exit command mode.

### TRIM commands

`trim free` scans the FAT, finds contiguous runs of free clusters, and issues
one erase command per run. Useful before a long logging session to improve
write performance on flash cards that benefit from pre-erased blocks.

`trim full` erases the data region of the partition. MBR, VBR, and FAT are
preserved. The volume is still mountable but all file content is gone.

`trim fuller` erases from LBA 0 to the end of the partition. The card must be
reformatted before use. Use when decommissioning a card.

All three TRIM commands are best-effort. SD cards are not required to honour
erase commands; the firmware does not verify the result.

## Factory reset

Hold the RX pin low at power-up to reset all settings to defaults. The STAT
LED blinks 5 times to confirm. Disabled by setting `CFG_FL_IGNORE_RX_RST` in
the flags EEPROM byte.

## Error codes

The STAT LED blinks `n` times, pauses 2 seconds, and repeats.

| Blinks | Meaning |
|---|---|
| 3 | SD card init failed |
| 4 | FAT32 mount failed |
| 5 | Log file open/create failed |
| 6 | Stack canary clobbered (overflow) |

## EEPROM layout

| Offset | Size | Field |
|---|---|---|
| 0 | 4 B | magic `0x4F4C3200` |
| 4 | 4 B | baud rate |
| 8 | 1 B | escape character (default `0x1A`) |
| 9 | 1 B | escape count (default 3) |
| 10 | 2 B | log file number |
| 12 | 1 B | flags |
| 13 | 1 B | prealloc size code |
| 14 | 2 B | reserved |
| 16 | 2 B | recovery: bytes_written low 16 bits |
| 18 | 2 B | recovery: bytes_written high 16 bits |

## Source layout

```
include/
  config.h        EEPROM config struct and accessors
  uart.h          ring buffer, UART init, TX helpers
  spi.h           SPI master, pin defines, transfer inline
  sd.h            SD SPI protocol (init, read, write, erase)
  fat32.h         FAT32 public API
  fat32_priv.h    FAT32 internal types and state (not for external use)
  log.h           logging loop, flush, canary check
  repl.h          command mode entry point
  util.h          delay_ms, delay_us

src/
  main.c          init sequence, main loop
  config.c        EEPROM read/write, prealloc table
  uart.c          USART0 init, RX ISR, TX
  spi.c           SPI master init, byte/buffer transfer
  sd.c            SD SPI command protocol
  fat32_state.c   vol and logfile state, FAT entry I/O
  fat32_mount.c   MBR and VBR parsing
  fat32_log.c     open/resume log file, append sector, flush
  fat32_vol.c     volume layout getters
  fat32_trim.c    trim free / full / fuller
  log.c           ring drain, escape detection, idle flush
  repl.c          command mode UART loop and dispatch
```

## Building

Requires `avr-gcc`, `avr-libc`, and `avrdude`.

```sh
make                        # build openlog.hex
make flash PORT=/dev/ttyUSB0  # flash via stk500v1 bootloader
make size                   # print section sizes
make clean
```

Default flash port is `/dev/ttyUSB0`. Override with `PORT=`.

## Supported baud rates

9600, 19200, 38400, 57600, 115200. Any other value stored in EEPROM is
treated as 115200 at boot.
