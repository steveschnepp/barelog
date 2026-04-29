#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>
#include <util/delay.h>

/* busy-delay approximately N milliseconds at 16 MHz */
static inline void delay_ms(uint16_t ms)
{
	while (ms--)
		_delay_ms(1);
}

/* busy-delay approximately N microseconds at 16 MHz */
static inline void delay_us(uint16_t us)
{
	while (us--)
		_delay_us(1);
}

#endif /* UTIL_H */
