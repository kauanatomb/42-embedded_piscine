#include "uart.h"

// each event on hardware has its own vector number
// 19 - 1 -> USART Rx Complete 
void __vector_18(void) __attribute__((signal, used));
void __vector_18(void)
{
    char c = UDR0;
    // transform char
    if (c <= 'z' && c >= 'a')
        c -= 32;
    else if (c <= 'Z' && c >= 'A')
        c += 32;
    uart_tx(c);
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

int main(void)
{
    uart_init();
    while (1) {}
}