/*
** References:
** AHT20 Data Sheet: https://files.seeedstudio.com/wiki/Grove-AHT20_I2C_Industrial_Grade_Temperature_and_Humidity_Sensor/AHT20-datasheet-2020-4-16.pdf
*/

#include <avr/io.h>

// debug mode allows to explicit I2C & AHT function calls
#ifdef DEBUG
# define TX_BUF_SIZE	128
#else
# define TX_BUF_SIZE	64
#endif

#define NL			"\r\n"

#define	ERR_MSG		"Error"NL

// Define NULL as defined in the STanDard LIBrary
#define NULL	((void*)0)

// Hard code the values for F_CPU & BAUD if undefined during compilation
#ifndef F_CPU
# define F_CPU	16000000UL
#endif
#ifndef BAUD
# define BAUD 	115200
#endif

// I2C device address of the AHT20 (c.f. AHT20 DS, p.8, section 5.3)
#define AHT20	0x38

// AHT20 phases
typedef enum e_aht20_phase
{
	AHT20_BOOT,     // waiting 40ms after power-on
	AHT20_BUSY,     // I2C transaction in progress, timer idles
	AHT20_CONVERT,  // waiting 80ms for conversion (2 ticks × 40ms)
	AHT20_POLL,     // waiting 2s between measurements (50 ticks × 40ms)
}   t_aht20_phase;

// Global state & tick variables to monitor AHT20 phase and timer1 tick
static volatile t_aht20_phase   g_phase = AHT20_BOOT;
static volatile uint8_t         g_tick  = 0;

// Receive buffer for the 6 raw bytes read from the AHT20 sensor.
static volatile	uint8_t	g_aht20_rx[6];

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

// Single global transaction — only one I2C operation can be in flight at a time.
static volatile t_i2c	g_i2c;

static volatile uint8_t	g_hex_status[6] = {'0', 'x', 0, 0, '\r', '\n'};

static volatile uint8_t	tx_buf[TX_BUF_SIZE];
static volatile uint8_t	tx_head = 0;
static volatile uint8_t	tx_tail = 0;

/*
** Configure UART for TX-only at configured baud rate.
** U2X0 enables double-speed mode for better accuracy at 115200.
** UDRIE0 (TX interrupt) is enabled only when data is queued (lazy approach).
*/
static void	uart_init(void)
{
	uint16_t	ubrr = F_CPU / (8 * BAUD) - 1;

	UBRR0H = (uint8_t)(ubrr >> 8); // Set baud rate high byte
	UBRR0L = (uint8_t)ubrr; // Set baud rate low byte

	UCSR0A = (1 << U2X0); // Enable double speed mode for more accurate baud rate
	UCSR0B = (1 << TXEN0); // Enable transmitter, no receiver

	UCSR0C =  (0 << UMSEL01) | (0 << UMSEL00) // Asynchronous USART
			| (0 << UPM01)   | (0 << UPM00) // No parity
			| (0 << USBS0) // 1 stop bit
			| (1 << UCSZ01)  | (1 << UCSZ00); // 8 data bits
}

/*
** Ring-buffer wrap: advance index with modulo using bit-mask.
** Only works because TX_BUF_SIZE is power-of-two (64).
*/
static uint8_t	tx_next(uint8_t idx)
{
	return ((uint8_t)((idx + 1) & (TX_BUF_SIZE - 1)));
}

/*
** Enable UART Data Register Empty interrupt.
** Safe to call multiple times (setting already-set bit is a no-op).
** This wakes the TX consumer ISR if queue was idle.
*/
static void	uart_tx_kick(void)
{
	UCSR0B |= (1 << UDRIE0);
}

/*
** Enqueue single byte; returns 0 if buffer full, 1 if success.
** Non-blocking: never waits. Returns immediately to avoid ISR latency.
** Kicks UDRIE0 to ensure consumer wakes up if idle.
*/
static uint8_t	uart_tx_push(uint8_t c)
{
	uint8_t	next = tx_next(tx_head);

	if (next == tx_tail)
		return (0);
	tx_buf[tx_head] = c;
	tx_head = next;
	uart_tx_kick();
	return (1);
}

/*
** Enqueue variable-length buffer; drops silently if insufficient space.
** Non-blocking to keep ISR latency bounded; samples lost if queue full.
** Each call to uart_tx_push checks free slots and kicks TX ISR.
*/
static void	uart_tx_push_buf(volatile uint8_t *data, uint8_t len)
{
	uint8_t	free_slots;
	uint8_t	i;

	if (tx_head >= tx_tail)
		free_slots = (uint8_t)(TX_BUF_SIZE - (tx_head - tx_tail) - 1);
	else
		free_slots = (uint8_t)(tx_tail - tx_head - 1);
	if (free_slots < len)
		return ;

	i = 0;

	while (i < len)
	{
		uart_tx_push(data[i]);
		++i;
	}
}

/*
** Configure Timer1: CTC mode at 25 Hz (40 ms period).
** Compare A interrupt fires every 10000 counts (prescaler 64: 16M/64 = 250K counts/s).
** ISR kicks ADC conversion to maintain steady sensor sampling rate.
*/
static void timer1_init(void)
{
	TCCR1A = 0b00000000;
	TCCR1B = 0b00001011;

	TCNT1 = 0;

	OCR1A = 10000;

	TIMSK1 = (1 << OCIE1A);
}

/*
** Configure the I2C peripheral for 100 kHz operation.
** TWSR prescaler bits (TWPS1:0) cleared, prescaler = 1.
** TWBR set to 72, SCL = F_CPU / (16 + 2 × TWBR × 1) = 100 kHz @ 16 MHz.
** TWEN enables the I2C module; TWIE is left off until a transaction starts.
*/
static void	i2c_init(void)
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
static void	i2c_load(
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
static void	i2c_start(void)
{
	#ifdef DEBUG 
		uart_tx_push_buf((volatile uint8_t *)"I2C start called...\r\n", 21);
	#endif

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
static void	i2c_stop(void)
{
	#ifdef DEBUG 
		uart_tx_push_buf((volatile uint8_t *)"I2C stop called...\r\n", 20);
	#endif
	TWCR = (1 << TWEN) | (1 << TWINT) | (1 << TWSTO);
}

/*
** Callback: invoked by the TWI ISR once the 6-byte read transaction completes.
**
** On error: resets phase to AHT20_BOOT so the init sequence restarts.
** On success: advances phase to AHT20_POLL to wait 2s before next trigger.
*/
static void on_aht20_data(uint8_t *data, uint8_t len, uint8_t ok)
{
	#ifdef DEBUG 
		uart_tx_push_buf((volatile uint8_t *)"AHT20 data called...\r\n", 22);
	#endif

	(void)data; (void)len;
	if (!ok)
	{
		g_phase = AHT20_BOOT;
		uart_tx_push_buf((volatile uint8_t *)ERR_MSG, sizeof(ERR_MSG) - 1);
		return ;
	}
	g_phase = AHT20_POLL;
}

/*
** Callback: invoked by the TWI ISR once the trigger command (0xAC 0x33 0x00)
** has been successfully transmitted to the AHT20.
** The sensor now starts an internal conversion that takes up to 80 ms.
** Resets g_tick and advances phase to AHT20_CONVERT so the timer ISR
** will initiate the 6-byte read after 2 ticks (2 × 40 ms = 80 ms).
**
** On error: resets phase to AHT20_BOOT to restart the init sequence.
*/
static void on_aht20_triggered(uint8_t *data, uint8_t len, uint8_t ok)
{
	#ifdef DEBUG
		uart_tx_push_buf((volatile uint8_t *)"AHT20 trigger called...\r\n", 25);
	#endif

	(void)data; (void)len;
	if (!ok)
	{
		g_phase = AHT20_BOOT;
		return ;
	}
	// Open the 80ms window — timer ISR will read when 2 ticks elapse
	g_tick  = 0;
	g_phase = AHT20_CONVERT;
}

/*
** Callback: invoked by the TWI ISR once the initialisation command
** (0xBE 0x08 0x00) has been successfully transmitted to the AHT20.
** Immediately chains into the first measurement trigger without waiting
** for another timer tick, since the sensor is already ready after init.
** The trigger callback (on_aht20_triggered) then opens the 80 ms window.
**
** On error: resets phase to AHT20_BOOT to retry initialisation on the
** next timer tick (40 ms later).
*/
static void on_aht20_init(uint8_t *data, uint8_t len, uint8_t ok)
{
	#ifdef DEBUG
		uart_tx_push_buf((volatile uint8_t *)"AHT20 init called...\r\n", 22);
	#endif
	static const uint8_t trigger[] = {0xAC, 0x33, 0x00};

	(void)data; (void)len;
	if (!ok)
	{
		g_phase = AHT20_BOOT;
		return ;
	}

	// Immediately trigger — on_aht20_triggered will open the 80ms window
	g_phase = AHT20_BUSY;
	i2c_load(AHT20, trigger, 3, NULL, 0, on_aht20_triggered);
	i2c_start();
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

void	__vector_11(void) __attribute__((signal, used));
void	__vector_19(void) __attribute__((signal, used));
void	__vector_24(void) __attribute__((signal, used));

/*
** Timer1 Compare Match A ISR — fires every 40 ms (25 Hz).
** Acts as the time base for the entire AHT20 sampling loop.
** On each tick, increments g_tick and checks g_phase to decide
** whether to advance to the next stage of the sequence:
**
**   AHT20_BOOT    g_tick >= 1  ( 1 × 40ms =  40ms)
**     - send initialisation command (0xBE 0x08 0x00)
**     - on_aht20_init callback immediately chains the first trigger
**
**   AHT20_CONVERT g_tick >= 2  ( 2 × 40ms =  80ms)
**     - sensor conversion window has elapsed (AHT20 DS p. 8, section 5.4)
**     - read 6 bytes; on_aht20_data decodes and sends result over UART
**
**   AHT20_POLL    g_tick >= 50 (50 × 80ms = 2000ms)
**     - 2s inter-measurement delay elapsed
**     - send trigger command (0xAC 0x33 0x00) to start next conversion
**
**   AHT20_BUSY    (any tick)
**     - an I2C transaction is in flight; do nothing.
**     - the TWI ISR callback will advance g_phase when done.
**
** g_tick is reset to 0 whenever a phase transition fires, so it always
** counts ticks elapsed since the last transition, not since boot.
*/
void    __vector_11(void)
{
	static const uint8_t    init_cmd[]    = {0xBE, 0x08, 0x00};
	static const uint8_t    trigger_cmd[] = {0xAC, 0x33, 0x00};

	g_tick++;

	if (g_phase == AHT20_BOOT && g_tick >= 1)          // 1 × 40ms = 40ms
	{
		g_tick = 0;
		g_phase = AHT20_BUSY;
		i2c_load(AHT20, init_cmd, 3, NULL, 0, on_aht20_init);
		i2c_start();
	}
	else if (g_phase == AHT20_CONVERT && g_tick >= 2)  // 2 × 40ms = 80ms
	{
		g_tick = 0;
		g_phase = AHT20_BUSY;
		i2c_load(AHT20, NULL, 0, (uint8_t *)g_aht20_rx, 6, on_aht20_data);
		i2c_start();
	}
	else if (g_phase == AHT20_POLL && g_tick >= 50)    // 25 × 80ms = 2s
	{
		g_tick = 0;
		g_phase = AHT20_BUSY;
		i2c_load(AHT20, trigger_cmd, 3, NULL, 0, on_aht20_triggered);
		i2c_start();
	}
	// AHT20_BUSY: I2C transaction in flight — TWI ISR owns g_phase
}

/*
** USART Data Register Empty interrupt: drain TX queue to hardware.
** Called each time UDR0 becomes ready (every ~87 µs @ 115200 baud).
** Sends one byte per ISR; auto-disables UDRIE0 when queue empty.
** Paces output at UART bitrate with zero busy-waiting.
*/
void	__vector_19(void) // USART data register empty ISR
{
	if (tx_head == tx_tail)
	{
		UCSR0B &= ~(1 << UDRIE0);
		return ;
	}
	UDR0 = tx_buf[tx_tail];
	tx_tail = tx_next(tx_tail);
}uart_tx_push_buf(g_hex_status, 6);

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

int	main(void)
{
	uart_init();
	i2c_init();
	timer1_init();
	SREG = (1 << 7);

	uart_tx_push_buf((volatile uint8_t *)"Starting up...\r\n", 16);
	while (1)
		;
}
