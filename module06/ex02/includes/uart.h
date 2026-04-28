#ifndef UART_H
#define UART_H

#include <avr/io.h>

#ifndef UART_BAUDRATE
# define UART_BAUDRATE 115200
#endif

// UBRR = (F_CPU + BAUDRATE * 8) / (BAUDRATE * 16) - 1
// Adding BAUDRATE*8 before dividing rounds to nearest integer
// instead of truncating. At 16MHz/115200: (16000000 + 921600) / 1843200 - 1 = 8
// Plain truncation gives 7, causing baud error and corrupted data.
#define UBRR_VALUE ((F_CPU + UART_BAUDRATE * 8) / (UART_BAUDRATE * 16) - 1)

# define TX_BUF_SIZE 64

void	uart_init(void);
void	uart_tx_push_buf(volatile uint8_t *data, uint8_t len);

#endif