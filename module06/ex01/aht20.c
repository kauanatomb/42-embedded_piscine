#include "includes/aht20.h"
#include "includes/uart.h"
#include "includes/i2c.h"

// Global state & tick variables to monitor AHT20 phase and timer1 tick
static volatile t_aht20_phase   g_phase = AHT20_BOOT;
static volatile uint8_t         g_tick  = 0;

// Receive buffer for the 6 raw bytes read from the AHT20 sensor.
static volatile	uint8_t	g_aht20_rx[6];

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

static void	print_hex_value(uint8_t *data, uint8_t len)
{
	uint8_t	hex_byte[3];
	uint8_t	i = 0;

	hex_byte[2] = ' ';
	while (i < len)
	{
		hex_byte[0] = get_hex_char(data[i] >> 4);
		hex_byte[1] = get_hex_char(data[i] & 0x0F);
		if (i + 1 == len)
			hex_byte[2] = 0;
		uart_tx_push_buf(hex_byte, 3);
		++i;
	}
	uart_tx_push_buf((volatile uint8_t *)NL, 2);
}

/*
** Callback: invoked by the TWI ISR once the 6-byte read transaction completes.
** Decodes the raw sensor payload into temperature and humidity and pushes
** a formatted "Temperature: XX.X, Humidity: XX.X\r\n" string into the UART TX ring buffer.
**
** Raw data layout (AHT20 DS, p. 8, section 5.4):
**   data[0]       : status byte (ignored here, checked before triggering)
**   data[1]       : humidity   [19:12]
**   data[2]       : humidity   [11:4]
**   data[3] [7:4] : humidity   [3:0]
**   data[3] [3:0] : temperature[19:16]
**   data[4]       : temperature[15:8]
**   data[5]       : temperature[7:0]
**
** Conversion (fixed-point, ×10 to preserve one decimal without floats):
**   T(°C)  × 10 = raw_temp × 2000 / 1048576 - 500
**   RH(%)  × 10 = raw_hum  × 1000 / 1048576
** (c.f. ADHT20 DS, p.9, section 6.1)
**
** On error: resets phase to AHT20_BOOT so the init sequence restarts.
** On success: advances phase to AHT20_POLL to wait 2s before next trigger.
*/
void on_aht20_data(uint8_t *data, uint8_t len, uint8_t ok)
{
	(void)len;
	if (!ok)
	{
		g_phase = AHT20_BOOT;
		uart_tx_push_buf((volatile uint8_t *)ERR_MSG, sizeof(ERR_MSG) - 1);
		return ;
	}
	print_hex_value(data, len);
	g_tick  = 0;
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
void on_aht20_triggered(uint8_t *data, uint8_t len, uint8_t ok)
{
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
** On success, it marks the init as complete and moves the state machine
** to AHT20_INIT so the timer ISR can send the first trigger on the next
** 40 ms tick.
**
** On error: resets phase to AHT20_BOOT to retry initialisation on the
** next timer tick (40 ms later).
*/

void on_aht20_init(uint8_t *data, uint8_t len, uint8_t ok)
{
	(void)data; (void)len;
	
	if (!ok)
	{
		g_phase = AHT20_BOOT;
		return ;
	}
	g_tick = 0;
	g_phase = AHT20_INIT;
}

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
	else if (g_phase == AHT20_INIT && g_tick >= 1)     // 1 × 40ms = 40ms
	{
		g_tick = 0;
		g_phase = AHT20_BUSY;
		i2c_load(AHT20, trigger_cmd, 3, NULL, 0, on_aht20_triggered);
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