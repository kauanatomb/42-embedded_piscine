#include <avr/io.h>
#include <util/delay.h>

#define RED PD5
#define GREEN PD6
#define BLUE PD3

void init_rgb() {
    // config timer0 and timer2
    /* PD5 (OC0B) as output 
       PD6 (0C0A) as output
       PD3 (OC2B) as output
    */
    DDRD |= (1 << BLUE);
    DDRD |= (1 << RED);
    DDRD |= (1 << GREEN);

    // setup timer0
    TCCR0A = (1 << WGM01) | (1 << WGM00) | (1 << COM0B1) | (1 << COM0A1);
    TCCR0B = (1 << CS01);

    // setup timer2
    TCCR2A = (1 << WGM21) | (1 << WGM20) | (1 << COM2B1);
    TCCR2B = (1 << CS21);

    OCR0A = 0;
    OCR0B = 0;
    OCR2B = 0;
}

void set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    OCR0B = r;  // PD5
    OCR0A = g;  // PD6
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
        for (uint8_t i = 0; i < 256; i++)
        {
            wheel(i);
            _delay_ms(10);
        }
    }
}
