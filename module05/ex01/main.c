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
}

static void adc_init(void)
{
    /* AVCC ref, ADLAR=1 (8-bit ADCH), chanel 0 (p.257) */
    ADMUX  = (1 << REFS0) | (1 << ADLAR);
    /* Enable, prescaler 128 -> 125kHz 16MHz (p.249, 258)*/
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

static uint8_t adc_read(uint8_t channel)
{
    // select channel preserv REFS0 and ADLAR
    ADMUX = (ADMUX & 0xF8) | (channel & 0x07);
    // (p.258)
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC))
        ;
    // (p.259)
    return ADCH; /* 8 MSBs */
}

void uart_puts(char *s) {
    while(*s)
        uart_putc(*s++);
}

int main(void)
{
    uart_init();
    adc_init();

    while (1) {
        uint8_t rv1 = adc_read(0);
        uint8_t ldr = adc_read(1);
        uint8_t ntc = adc_read(2);
        uart_print_hex(rv1);
        uart_puts(", ");
        uart_print_hex(ldr);
        uart_puts(", ");
        uart_print_hex(ntc);
        uart_puts("\r\n");
        _delay_ms(20);
    }
}