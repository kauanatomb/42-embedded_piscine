#ifndef I2C_H
#define I2C_H

#include <avr/io.h>

// I2C status codes (TWSR masked with 0xF8)
typedef enum e_i2c_status
{
	I2C_START_OK   = 0x08, // START condition transmitted, bus acquired
	I2C_REPEAT_OK  = 0x10, // Repeated START transmitted (switching MT→MR mid-transaction)
	I2C_SLAW_ACK   = 0x18, // MT: SLA+W transmitted, slave ACK'd → ready to send data
	I2C_SLAW_NACK  = 0x20, // MT: SLA+W transmitted, slave NACK'd → slave absent or busy
	I2C_DATAT_ACK  = 0x28, // MT: data byte transmitted, slave ACK'd → send next byte or STOP
	I2C_SLAR_ACK   = 0x40, // MR: SLA+R transmitted, slave ACK'd → ready to receive data
	I2C_SLAR_NACK  = 0x48, // MR: SLA+R transmitted, slave NACK'd → slave absent or busy
	I2C_DATAR_ACK  = 0x50, // MR: data byte received, ACK sent to slave → more bytes follow
	I2C_DATAR_NACK = 0x58, // MR: data byte received, NACK sent to slave → last byte, issue STOP
	I2C_UNKNOWN    = 0xFF  // Unexpected status: bus error or arbitration lost
}	t_i2c_status;

/*
** Active I2C transaction descriptor.
** Loaded by i2c_load() before every transaction; consumed entirely by the TWI ISR.
** All fields must be stable for the full duration of the transaction —
** never modify while g_phase == AHT20_BUSY.
*/
typedef struct	s_i2c
{
	uint8_t			sla;       // 7-bit slave address (shifted + R/W bit added in ISR)

	const uint8_t	*tx_buf;   // pointer to bytes to transmit (NULL for read-only)
	uint8_t			tx_len;    // total number of bytes to transmit
	uint8_t			tx_idx;    // index of next byte to send (advanced by ISR)

	uint8_t			*rx_buf;   // pointer to receive buffer (NULL for write-only)
	uint8_t			rx_len;    // total number of bytes to receive
	uint8_t			rx_idx;    // index of next byte to store (advanced by ISR)

	// Callback invoked by ISR on completion or error.
	// data: pointer to rx_buf (NULL on write-only or error)
	// len:  number of bytes actually received
	// ok:   1 = success, 0 = slave NACK'd or bus error
	void			(*on_complete)(uint8_t *data, uint8_t len, uint8_t ok);
}	t_i2c;

// Define NULL to not use stdlib
#define NULL	((void*)0)

void	i2c_init(void);
void	i2c_load(
	uint8_t			sla,
	const uint8_t*	tx,
	uint8_t			tx_len,
	uint8_t*		rx,
	uint8_t			rx_len,
	void			(*on_complete)(uint8_t *, uint8_t, uint8_t));
void	i2c_start(void);
void	i2c_stop(void);
void	__vector_24(void) __attribute__((signal, used));

#endif