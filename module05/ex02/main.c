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

// 0-1023 max 4 digits
static void uart_print_dec(uint16_t v)
{
    char buf[5];
    uint8_t i = 0;

    if (v == 0) {
        uart_putc('0');
        return ;
    }
    while (v > 0) {
        buf[i++] = '0' + (v % 10);
        v /= 10;
    }
    while(i--)
        uart_putc(buf[i]);
}

static void adc_init(void)
{
    /* AVCC ref, ADLAR=0 (right adjust, default), 10bit in ADCL+ADCH, chanel 0 (p.257) */
    ADMUX  = (1 << REFS0);
    /* Enable, prescaler 128 -> 125kHz 16MHz (p.249, 258)*/
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

static uint16_t adc_read(uint8_t channel)
{
    // select channel and preserv REFS0
    ADMUX = (ADMUX & 0xF8) | (channel & 0x07);
    // (p.258)
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC))
        ;
    // (p.259) In right-adjust mode, read ADCL first, then ADCH.
    return (uint16_t)ADCL | ((uint16_t)ADCH << 8);
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
        uint16_t rv1 = adc_read(0);
        uint16_t ldr = adc_read(1);
        uint16_t ntc = adc_read(2);
        uart_print_dec(rv1);
        uart_puts(", ");
        uart_print_dec(ldr);
        uart_puts(", ");
        uart_print_dec(ntc);
        uart_puts("\r\n");
        _delay_ms(20);
    }
}