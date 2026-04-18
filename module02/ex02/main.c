#include "uart.h"

// p.182 - USART initialization
void uart_init(void)
{
    // Split 12-bit UBRR value across high and low registers (p.204)
    UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
    UBRR0L = (uint8_t)(UBRR_VALUE);

    // Enable TX and RX (p.201)
    UCSR0B = (1 << TXEN0) | (1 << RXEN0);

    // UCSZ01:UCSZ00 = 1:1 -> 8 data bits, USBS0=0 -> 1 stop bit,
    // UPM01:UPM00 = 0:0 -> no parity (8N1 frame format) (p.203)
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

char uart_rx(void) {
    // Receive Complete (RXCn) indicates unread data present in the receive buffer (p.190)
    while (!(UCSR0A & (1<<RXC0)));
    return UDR0;
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

    while (1) {
        char c;
        c = uart_rx();
        uart_tx(c);
    }
}