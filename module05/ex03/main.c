#include "uart.h"

// #define TEMP_TOS 292
// #define TEMP_K_X100 0.99

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

static void uart_print_dec(int16_t v)
{
    char buf[7];
    uint8_t i = 0;
    int16_t abs_v;

    if (v < 0) {
        uart_putc('-');
        abs_v = -v;
    } else
        abs_v = v;

    if (abs_v == 0) {
        uart_putc('0');
        return ;
    }
    while (abs_v > 0) {
        buf[i++] = '0' + (abs_v % 10);
        abs_v /= 10;
    }
    while(i--)
        uart_putc(buf[i]);
}

static void adc_init(void)
{
    /* 1.1V ref for temp sensor (p.256), MUX[3:0] = 1000 channel 8 intern temp sensor (p.257) */
    ADMUX  = (1 << REFS1) | (1 << REFS0) | (1 << MUX3);
    /* Enable, prescaler 128 -> 125kHz 16MHz (p.249, 258)*/
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    /* remove first read, internal reference needs to be stable p.252 */
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
}

static uint16_t adc_read_temp(void)
{
    // (p.258)
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC))
        ;
    // (p.259)
    return ADC;
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
    uint16_t temp_raw = adc_read_temp();
    // int16_t temp = (int16_t)(((temp_raw - TEMP_TOS)) / TEMP_K_X100);
    int16_t temp = (temp_raw - 364);
    uart_print_dec(temp);
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