#include "uart.h"

// each event on hardware has its own vector number
// 19 - 1 -> USART Rx Complete 
void __vector_18(void) __attribute__((signal, used));
void __vector_18(void)
{
    char c = UDR0;
    if (c == '\r') {
        // finish actual string
        if (state == WAIT_USER) {
            user_buf[idx] = '\0';
            state = WAIT_PASS;
            idx = 0;
            uart_print("\r\n");
            uart_print("password: ");
        } else if (state == WAIT_PASS) {
            pass_buf[idx] = '\0';
            state = VALIDATING;
            idx = 0;
            uart_print("\r\n");
        }
        return;
    }

    if (c == BACKSPACE) {
        if (idx > 0) {
            idx--;
            uart_tx('\b');
            uart_tx(' ');
            uart_tx('\b');
        }
        return;
    }

    // ignore if full buffer
    if (idx >= BUF_SIZE - 1)
        return;

    if (state == WAIT_USER) {
        user_buf[idx++] = c;
        uart_tx(c); // echo normal
    } else if (state == WAIT_PASS) {
        pass_buf[idx++] = c;
        uart_tx('*'); // echo mask
    }
}

void uart_print(const char *s)
{
    while (*s)
        uart_tx(*s++);
}

// p.182 - USART initialization
void uart_init(void)
{
    // Split 12-bit UBRR value across high and low registers (p.204)
    UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
    UBRR0L = (uint8_t)(UBRR_VALUE);

    // Enable TX and RX and USART Receive Complete interrupt (p.201)
    UCSR0B = (1 << TXEN0) | (1 << RXEN0) | (1 << RXCIE0);

    // UCSZ01:UCSZ00 = 1:1 -> 8 data bits, USBS0=0 -> 1 stop bit,
    // UPM01:UPM00 = 0:0 -> no parity (8N1 frame format) (p.203)
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);

    // Enable global interrupts (p.212)
    SREG |= (1 << 7);
}

void uart_tx(char c)
{
    // Poll UDRE0 (TX buffer empty flag) before writing 
    // prevents overwriting a byte still being shifted out (p.186, 200, 212, 210)
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

void leds_init(void)
{
    DDRB |= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4);
}

void leds_blink(void)
{
    uint8_t i = 6;
    while (i--) {
        PORTB ^= (1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4);
        _delay_ms(300);
    }
}

int strcmp_myversion(const char *a, const volatile char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

int main(void)
{
    leds_init();
    uart_init();
    uart_print("Enter your login:\r\nusername: ");

    while (1) {
        if (state == VALIDATING) {
            if (strcmp_myversion(USERNAME, user_buf) == 0 &&
                strcmp_myversion(PASSWORD, pass_buf) == 0) {
                uart_print("Hello eval!\r\nShall we play a game?\r\n");
                leds_blink();
                state = DONE;
            } else {
                uart_print("Bad combination username/password\r\n");
                // reset
                state = WAIT_USER;
                idx = 0;
                uart_print("Enter your login:\r\nusername: ");
            }
        }
    }
}