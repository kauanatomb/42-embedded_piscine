#include "includes/aht20.h"
#include "includes/uart.h"
#include "includes/i2c.h"

// Global state & tick variables to monitor AHT20 phase and timer1 tick
static volatile t_aht20_phase   g_phase = AHT20_BOOT;
static volatile uint8_t         g_tick  = 0;

// Receive buffer for the 6 raw bytes read from the AHT20 sensor.
static volatile	uint8_t	g_aht20_rx[6];

/*
** Callback: invoked by the TWI ISR once the 6-byte read transaction completes.
**
** On error: resets phase to AHT20_BOOT so the init sequence restarts.
** On success: advances phase to AHT20_POLL to wait 2s before next trigger.
*/
void on_aht20_data(uint8_t *data, uint8_t len, uint8_t ok)
{
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
** Immediately chains into the first measurement trigger without waiting
** for another timer tick, since the sensor is already ready after init.
** The trigger callback (on_aht20_triggered) then opens the 80 ms window.
**
** On error: resets phase to AHT20_BOOT to retry initialisation on the
** next timer tick (40 ms later).
*/
void on_aht20_init(uint8_t *data, uint8_t len, uint8_t ok)
{
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