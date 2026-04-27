#include "includes/i2c.h"
#include "includes/uart.h"

// Single global transaction — only one I2C operation can be in flight at a time.
static volatile t_i2c	g_i2c;

static volatile uint8_t	g_hex_status[6] = {'0', 'x', 0, 0, '\r', '\n'};

/*
** Configure the I2C peripheral for 100 kHz operation.
** TWSR prescaler bits (TWPS1:0) cleared, prescaler = 1.
** TWBR set to 72, SCL = F_CPU / (16 + 2 × TWBR × 1) = 100 kHz @ 16 MHz.
** TWEN enables the I2C module; TWIE is left off until a transaction starts.
*/
void	i2c_init(void)
{
	TWSR = 0;
	TWBR = 72;

	TWCR = (1 << TWEN);
}

/*
** Load the transaction descriptor into g_i2c before calling i2c_start().
** Does NOT touch TWCR — safe to call at any time before the transaction begins.
** Pass tx=NULL/tx_len=0 for a read-only transaction.
** Pass rx=NULL/rx_len=0 for a write-only transaction.
*/
void	i2c_load(
	uint8_t			sla,
	const uint8_t*	tx,
	uint8_t			tx_len,
	uint8_t*		rx,
	uint8_t			rx_len,
	void			(*on_complete)(uint8_t *, uint8_t, uint8_t)
	)
{
	g_i2c.sla         = sla;

	g_i2c.tx_buf      = tx;
	g_i2c.tx_len      = tx_len;
	g_i2c.tx_idx      = 0;

	g_i2c.rx_buf      = rx;
	g_i2c.rx_len      = rx_len;
	g_i2c.rx_idx      = 0;
	
	g_i2c.on_complete = on_complete;
}

/*
** Assert a START condition on the I2C bus and enable the TWI interrupt.
** Must be called after i2c_load() has populated g_i2c.
** Spins until any pending STOP has completed: writing TWCR with TWSTO=0
** while a STOP is in progress silently cancels it, leaving the bus hung.
** TWSTO is hardware-cleared by the peripheral once the STOP is transmitted
** (~10 µs at 100 kHz — at most ~160 cycles). From this point the TWI ISR
** (__vector_24) owns the bus and drives the full transaction autonomously.
*/
void	i2c_start(void)
{
	while (TWCR & (1 << TWSTO))
		;
	TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWSTA);
}

/*
** Release the I2C bus by transmitting a STOP condition.
** Clears TWIE so no further TWI interrupts fire after this point.
** TWSTO is cleared automatically by hardware once the STOP is transmitted;
** do not poll it here — i2c_start() guards against re-entry.
*/
void	i2c_stop(void)
{
	TWCR = (1 << TWEN) | (1 << TWINT) | (1 << TWSTO);
}

/*
** Convert 4-bit value (0-15) to ASCII hex character (0-9, a-f).
** Used to format 8-bit ADC results as two hex chars for UART output.
*/
static uint8_t	get_hex_char(uint8_t val)
{
	if (val < 10)
		return ('0' + val);
	else
		return ('a' + (val - 10));
}

/*
** TWI (I2C) interrupt ISR — fires every time the TWI hardware completes
** an operation and sets TWINT. While TWINT is set, SCL is held LOW by
** the hardware: the bus is stalled and waiting for the CPU to act.
** This ISR reads the status code (TWSR & 0xF8), decides the next action,
** prepares TWDR if needed, then writes TWCR with TWINT=1 to release SCL
** and let the transaction proceed. One ISR invocation per bus event.
**
** Full write-then-read sequence (e.g. AHT20 trigger + 6-byte read):
**
**   i2c_start() asserts TWSTA
**     ISR: START_OK  (0x08) : send SLA+W
**     ISR: SLAW_ACK  (0x18) : send first data byte
**     ISR: DATAT_ACK (0x28) : send next byte until tx_idx == tx_len
**     ISR: DATAT_ACK (0x28) : nothing left to write, assert TWSTA again
**     ISR: REPEAT_OK (0x10) : send SLA+R
**     ISR: SLAR_ACK  (0x40) : arm receiver, set TWEA if >1 byte follows
**     ISR: DATAR_ACK (0x50) : store byte, re-arm; NACK on second-to-last
**     ISR: DATAR_NACK(0x58) : store last byte, STOP, call on_complete()
**
** Write-only sequence (e.g. init or trigger command):
**   … same up to last DATAT_ACK: STOP, call on_complete(NULL, 0, ok=1)
**
** TWEA controls whether the hardware ACKs or NACKs the incoming byte:
**   TWEA=1 : ACK  : slave keeps sending (more bytes expected)
**   TWEA=0 : NACK : slave stops after this byte (last byte)
*/
void	__vector_24(void)
{
	uint8_t	status = TWSR & 0xF8;
	
	g_hex_status[2] = get_hex_char(status >> 4);
	g_hex_status[3] = get_hex_char(status & 0x0F);

	uart_tx_push_buf(g_hex_status, 6);
	switch (status)
	{
		// Bus acquired (fresh START or repeated START after a write phase).
		// Decide whether to address in write mode (SLA+W) or read mode (SLA+R):
		// if bytes remain to be sent → SLA+W; all writes done → SLA+R.
		case I2C_START_OK:
		case I2C_REPEAT_OK:
			if (g_i2c.tx_idx < g_i2c.tx_len)
				TWDR = (g_i2c.sla << 1) | 0;  // SLA+W: master transmitter
			else
				TWDR = (g_i2c.sla << 1) | 1;  // SLA+R: master receiver
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
			break;

		// Slave acknowledged our address (write) or the last data byte we sent.
		// Three sub-cases:
		//   a) more bytes to send : load next byte into TWDR and continue
		//   b) write done, read phase follows : assert repeated START to
		//      re-address the slave in read mode without releasing the bus
		//   c) write-only transaction complete : STOP and invoke callback
		case I2C_SLAW_ACK:
		case I2C_DATAT_ACK:
			if (g_i2c.tx_idx < g_i2c.tx_len)
			{
				TWDR = g_i2c.tx_buf[g_i2c.tx_idx++];
				TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT);
			}
			else if (g_i2c.rx_len > 0)
				TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | (1 << TWSTA);
			else
			{
				i2c_stop();
				if (g_i2c.on_complete)
					g_i2c.on_complete(NULL, 0, 1);
			}
			break;

		// Slave acknowledged our address (read): receiver is now armed.
		// Set TWEA=1 to ACK (more bytes follow) or TWEA=0 to NACK (last byte).
		case I2C_SLAR_ACK:
		{
			uint8_t ack = (g_i2c.rx_len > 1) ? (1 << TWEA) : 0;
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | ack;
			break;
		}

		// A data byte was received and ACK was sent to the slave.
		// Store the byte, then decide ACK/NACK for the next one:
		// ACK if more than 1 byte still expected, NACK if this is second-to-last
		// (hardware sends NACK during the *next* receive to stop the slave).
		case I2C_DATAR_ACK:
		{
			g_i2c.rx_buf[g_i2c.rx_idx++] = TWDR;
			uint8_t remaining = g_i2c.rx_len - g_i2c.rx_idx;
			uint8_t ack = (remaining > 1) ? (1 << TWEA) : 0;
			TWCR = (1 << TWEN) | (1 << TWIE) | (1 << TWINT) | ack;
			break;
		}

		// Last byte received (NACK was sent). Store it, release the bus,
		// and invoke the callback with the full receive buffer.
		case I2C_DATAR_NACK:
			g_i2c.rx_buf[g_i2c.rx_idx++] = TWDR;
			i2c_stop();
			if (g_i2c.on_complete)
				g_i2c.on_complete(g_i2c.rx_buf, g_i2c.rx_idx, 1);
			break;

		// Slave not found (NACK on address) or unrecoverable bus error.
		// Abort the transaction and notify the callback with ok=0.
		case I2C_SLAW_NACK:
		case I2C_SLAR_NACK:
		default:
			i2c_stop();
			if (g_i2c.on_complete)
				g_i2c.on_complete(NULL, 0, 0);
			break;
	}
}