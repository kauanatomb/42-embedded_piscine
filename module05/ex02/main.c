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

// each event on hardware has its own vector number
// 12 - 1 -> timer1 compare match A
void __vector_11(void) __attribute__((signal, used));
void __vector_11(void)
{
    uint16_t rv1 = adc_read(0);
    uint16_t ldr = adc_read(1);
    uint16_t ntc = adc_read(2);
    uart_print_dec(rv1);
    uart_puts(", ");
    uart_print_dec(ldr);
    uart_puts(", ");
    uart_print_dec(ntc);
    uart_puts("\r\n");
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