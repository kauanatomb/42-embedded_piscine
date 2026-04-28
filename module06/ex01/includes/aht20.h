#ifndef AHT20_H
#define AHT20_H

#include <avr/io.h>

// I2C device address of the AHT20 (c.f. AHT20 DS, p.8, section 5.3)
#define AHT20	0x38

#define NL			"\r\n"

// AHT20 phases
typedef enum e_aht20_phase
{
	AHT20_BOOT,     // waiting 40ms after power-on
	AHT20_INIT,     // init completed, wait 40ms before first trigger
	AHT20_BUSY,     // I2C transaction in progress, timer idles
	AHT20_CONVERT,  // waiting 80ms for conversion (2 ticks × 40ms)
	AHT20_POLL,     // waiting 2s between measurements (50 ticks × 40ms)
}   t_aht20_phase;

// Define NULL to not use stdlib
#define NULL	((void*)0)
#define	ERR_MSG		"Error\r\n"

void	__vector_11(void) __attribute__((signal, used));
void on_aht20_init(uint8_t *data, uint8_t len, uint8_t ok);
void on_aht20_triggered(uint8_t *data, uint8_t len, uint8_t ok);
void on_aht20_data(uint8_t *data, uint8_t len, uint8_t ok);

#endif