#ifndef UART_H
#define UART_H

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#define BUF_SIZE 32
#define USERNAME "eval"
#define PASSWORD "abc123"
#define BACKSPACE 127

typedef enum {
    WAIT_USER,
    WAIT_PASS,
    VALIDATING,
    DONE
} state_t;

static volatile state_t state = WAIT_USER;
static volatile char user_buf[BUF_SIZE];
static volatile char pass_buf[BUF_SIZE];
static volatile uint8_t idx = 0;

#ifndef UART_BAUDRATE
# define UART_BAUDRATE 115200
#endif

// UBRR = (F_CPU + BAUDRATE * 8) / (BAUDRATE * 16) - 1
// Adding BAUDRATE*8 before dividing rounds to nearest integer
// instead of truncating. At 16MHz/115200: (16000000 + 921600) / 1843200 - 1 = 8
// Plain truncation gives 7, causing baud error and corrupted data.
#define UBRR_VALUE ((F_CPU + UART_BAUDRATE * 8) / (UART_BAUDRATE * 16) - 1)

void uart_init(void);
void uart_tx(char c);
void uart_print(const char *s);
int     strcmp_myversion(const char *a, const volatile char *b);
void leds_init(void);
void leds_blink(void);

#endif