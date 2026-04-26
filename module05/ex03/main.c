#include "uart.h"

#define TEMP_TOS 324
#define TEMP_K_X100 122

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

int main(void)
{
    uart_init();
    adc_init();

    while (1) {
        uint16_t temp_raw = adc_read_temp();
        /* T = (ADC - TOS) / k, using integer math (k ~= 1.22) (p.256)*/
        int16_t temp = (int16_t)(((int32_t)(temp_raw - TEMP_TOS) * 100) / TEMP_K_X100);
        uart_print_dec(temp);
        uart_puts("\r\n");
        _delay_ms(20);
    }
}