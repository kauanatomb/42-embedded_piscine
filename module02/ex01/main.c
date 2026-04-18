#include "uart.h"

// each event on hardware has its own vector number
// 12 - 1 -> timer1 compare match A
void __vector_11(void) __attribute__((signal, used));
void __vector_11(void)
{
    uart_printstr("Hello World!\r\n");
}

// p.182 - USART initialization
void uart_init(void)
{
    // Split 12-bit UBRR value across high and low registers (p.204)
    UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
    UBRR0L = (uint8_t)(UBRR_VALUE);

    // Enable TX (p.201)
    UCSR0B = (1 << TXEN0);

    // UCSZ01:UCSZ00 = 1:1 -> 8 data bits, USBS0=0 -> 1 stop bit,
    // UPM01:UPM00 = 0:0 -> no parity (8N1 frame format) (p.203)
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void timer1_init(void)
{
    // CTC mode - WGM12=1 (p.145)
    TCCR1B = (1 << WGM12) | (1 << CS12) | (1 << CS10); // prescaler 1024

    // 16000000 / 1024 * 2Hz - 1 = 31249 (2 seconds)
    OCR1A = 31249;

    // Enable Timer1 compare match A interrupt (p.145)
    TIMSK1 = (1 << OCIE1A);

    // Enable global interrupts (p.20)
    SREG |= (1 << 7);
}

void uart_tx(char c)
{
    // Poll UDRE0 (TX buffer empty flag) before writing 
    // prevents overwriting a byte still being shifted out (p.186, 200, 212, 210)
    while (!(UCSR0A & (1 << UDRE0)));
    UDR0 = c;
}

void uart_printstr(const char *str)
{
    while (*str)
        uart_tx(*str++);
}

int main(void)
{
    uart_init();
    timer1_init();

    while (1) {}
}