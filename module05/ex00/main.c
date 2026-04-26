#include "uart.h"

static void uart_init(void)
{
    UBRR0H = (uint8_t)(UBRR_VALUE >> 8);
    UBRR0L = (uint8_t)(UBRR_VALUE);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00); /* 8N1 */
}

static void uart_putc(char c)
{
    while (!(UCSR0A & (1 << UDRE0)))
        ;
    UDR0 = c;
}

static void uart_print_hex(uint8_t v)
{
    const char hex[] = "0123456789abcdef";
    uart_putc(hex[v >> 4]);
    uart_putc(hex[v & 0x0F]);
    uart_putc('\r');
    uart_putc('\n');
}

static void adc_init(void)
{
    /* AVCC ref, ADLAR=1 (8-bit ADCH), chanel 0 (p.257) */
    ADMUX  = (1 << REFS0) | (1 << ADLAR);
    /* Enable, prescaler 128 -> 125kHz 16MHz (p.249, 258)*/
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

static uint8_t adc_read(void)
{
    // (p.258)
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC))
        ;
    // (p.259)
    return ADCH; /* 8 MSBs */
}

// each event on hardware has its own vector number
// 12 - 1 -> timer1 compare match A
void __vector_11(void) __attribute__((signal, used));
void __vector_11(void)
{
    uart_print_hex(adc_read());
}

static void timer1_init(void)
{
    /* CTC mode (WGM12 = 1) */
    TCCR1B = (1 << WGM12);
    /* Prescaler 64 (CS11=1, CS10=1) */
    TCCR1B |= (1 << CS11) | (1 << CS10);
    /* Compare value: (16MHz x 20ms) / (64 x 1000) - 1 = 5000 - 1 */
    OCR1A = 4999;
    /* Enable interrupt */
    TIMSK1 = (1 << OCIE1A);
    SREG |= (1 << 7);
}

int main(void)
{
    uart_init();
    adc_init();
    timer1_init();

    while (1) {}
}