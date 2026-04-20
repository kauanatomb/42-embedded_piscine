#include <avr/io.h>
#include <util/delay.h>

#define RED PD5
#define BLUE PD3
#define GREEN PD6

void init_rgb() {
    // config timer0
    /* PD5 (OC0B) as output */
    DDRD |= (1 << BLUE);
    DDRD |= (1 << RED);
    DDRD |= (1 << GREEN);

    /* TOP = 62499 (frequency 1Hz)
    ** F_CPU / Prescaler / Frequency - 1
    ** 16.000.000 / 256 / 1 - 1 = 62499 */
    ICR1 = 62499;

    /* Duty cycle 50% TOP
    ** 62499 * 0.10 = 6249 */
    OCR0B = 128;

    // timer0
    TCCR0A = (1 << WGM01) | (1 << WGM00);   // Fast PWM
    TCCR0B = (1 << CS01);                  // prescaler
    TCCR0A |= (1 << COM0A1) | (1 << COM0B1);

    // timer2
    TCCR2A = (1 << WGM21) | (1 << WGM20);
    TCCR2B = (1 << CS21);
    TCCR2A |= (1 << COM2B1);
}

void set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    OCR0A = r;  // PD6
    OCR0B = g;  // PD5
    OCR2B = b;  // PD3
}

void wheel(uint8_t pos) {
    pos = 255 - pos;
    if (pos < 85) {
        set_rgb(255 - pos * 3, 0, pos * 3);
    } else if (pos < 170) {
        pos = pos - 85;
        set_rgb(0, pos * 3, 255 - pos * 3);
    } else {
        pos = pos - 170;
        set_rgb(pos * 3, 255 - pos * 3, 0);
    }
}

int main() {
    init_rgb();
    while (1)
    {
        for (uint8_t i = 0; i < 255; i++)
        {
            wheel(i);
            _delay_ms(10);
        }
    }
}
