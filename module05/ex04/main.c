#include "uart.h"

#define ADC_RV1 0

static void adc_init(void)
{
    ADMUX = (0 << REFS1) | (1 << REFS0) | ADC_RV1;
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
}

static uint16_t adc_read(void)
{
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));
    return ADC;
}

static void pwm_init(void)
{
    /* Timer0: Fast PWM, PD5 (OC0B) PD6 (OC0A) */
    TCCR0A = (1 << COM0A1) | (1 << COM0B1) | (1 << WGM01) | (1 << WGM00);
    TCCR0B = (1 << CS00);  /* Prescaler 1 */
    
    /* Timer2: Fast PWM, PD3 (OC2B) */
    TCCR2A = (1 << COM2B1) | (1 << WGM21) | (1 << WGM20);
    TCCR2B = (1 << CS20);  /* Prescaler 1 */
    
    /* DDR */
    DDRD |= (1 << 5) | (1 << 6) | (1 << 3);
    DDRB |= (1 << 0) | (1 << 1) | (1 << 2) | (1 << 4);
}

static void set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    OCR0B = r;   /* RED PD5 */
    OCR0A = g;   /* GREEN PD6 */
    OCR2B = b;   /* BLUE PD3 */
}

static void update_gauge(uint16_t adc_value)
{
    uint8_t d1 = (adc_value >= 256) ? 1 : 0;
    uint8_t d2 = (adc_value >= 512) ? 1 : 0;
    uint8_t d3 = (adc_value >= 768) ? 1 : 0;
    uint8_t d4 = (adc_value >= 1023) ? 1 : 0;
    
    uint8_t portb = (d1 << 0) | (d2 << 1) | (d3 << 2) | (d4 << 4);
    PORTB = (PORTB & ~0x17) | portb;
}

void wheel(uint16_t pos)
{
    pos = 767 - pos;
    if (pos < 256) {
        set_rgb((uint8_t)(255 - pos), (uint8_t)pos, 0);
    } else if (pos < 512) {
        pos -= 256;
        set_rgb(0, (uint8_t)(255 - pos), (uint8_t)pos);
    } else {
        pos -= 512;
        set_rgb((uint8_t)pos, 0, (uint8_t)(255 - pos));
    }
}

// each event on hardware has its own vector number
// 12 - 1 -> timer1 compare match A
void __vector_11(void) __attribute__((signal, used));
void __vector_11(void)
{
    uint16_t adc = adc_read();
    uint16_t wheel_pos = (uint16_t)(((uint32_t)adc * 767) / 1023);
    wheel(wheel_pos);
    update_gauge(adc);
}

static void timer1_init(void)
{
    /* CTC mode (WGM12 = 1) */
    TCCR1B = (1 << WGM12);
    /* Prescaler 64 (CS11=1, CS10=1) */
    TCCR1B |= (1 << CS11) | (1 << CS10);
    /* Compare value: (16MHz x 10ms) / (64 x 1000) - 1 = 2500 - 1 */
    OCR1A = 2499;
    /* Enable interrupt */
    TIMSK1 = (1 << OCIE1A);
    SREG |= (1 << 7);
}

int main(void)
{
    adc_init();
    pwm_init();
    timer1_init();
    
    while (1) {}
}