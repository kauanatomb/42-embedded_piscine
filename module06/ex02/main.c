/*
** References:
** AHT20 Data Sheet: https://files.seeedstudio.com/wiki/Grove-AHT20_I2C_Industrial_Grade_Temperature_and_Humidity_Sensor/AHT20-datasheet-2020-4-16.pdf
*/

#include <avr/io.h>
#include "includes/uart.h"
#include "includes/i2c.h"

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

int	main(void)
{
	uart_init();
	i2c_init();
	timer1_init();
	SREG = (1 << 7);

	uart_tx_push_buf((volatile uint8_t *)"Starting...\r\n", 13);
	while (1)
		;
}
