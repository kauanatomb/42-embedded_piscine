#ifndef UART_H
#define UART_H

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#ifndef UART_BAUDRATE
# define UART_BAUDRATE 115200
#endif

typedef enum {
    WAIT_WRITING, // user is writing
    VALIDATING, // validation
} state_t;
static volatile state_t state = WAIT_WRITING;

// UBRR = (F_CPU + BAUDRATE * 8) / (BAUDRATE * 16) - 1
// Adding BAUDRATE*8 before dividing rounds to nearest integer
// instead of truncating. At 16MHz/115200: (16000000 + 921600) / 1843200 - 1 = 8
// Plain truncation gives 7, causing baud error and corrupted data.
#define UBRR_VALUE ((F_CPU + UART_BAUDRATE * 8) / (UART_BAUDRATE * 16) - 1)

// define led pins
#define RED PD5
#define GREEN PD6
#define BLUE PD3

//buffer hex
#define BUF_SIZE 10
static volatile uint8_t idx = 0;
static volatile char hex_buf[BUF_SIZE];

#endif