#ifndef UART_H
#define UART_H

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#ifndef UART_BAUDRATE
# define UART_BAUDRATE 115200
#endif

// UBRR = (F_CPU + BAUDRATE * 8) / (BAUDRATE * 16) - 1
// Adding BAUDRATE*8 before dividing rounds to nearest integer
// instead of truncating. At 16MHz/115200: (16000000 + 921600) / 1843200 - 1 = 8
// Plain truncation gives 7, causing baud error and corrupted data.
#define UBRR_VALUE ((F_CPU + UART_BAUDRATE * 8) / (UART_BAUDRATE * 16) - 1)

void uart_init(void);
char uart_rx(void);
void uart_tx(char c);

#endif