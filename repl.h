#ifndef REPL_H
#define REPL_H

/*
 * Enter command mode.
 * Disables RXCIE0, flushes pending log data, then loops reading and
 * dispatching commands from the UART until the reset command is issued.
 * On reset: enables the watchdog (15 ms) and spins — never returns.
 * Declared noreturn so the compiler can omit the dead return path in
 * log_process after the call.
 */
void repl_enter(void) __attribute__((noreturn));

#endif /* REPL_H */
