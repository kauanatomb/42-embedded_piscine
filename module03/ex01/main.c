#include <avr/io.h>
#include <util/delay.h>

#define RED PD5
#define BLUE PD3
#define GREEN PD6

int main() {
    // config out
    DDRD |= (1 << BLUE);
    DDRD |= (1 << RED);
    DDRD |= (1 << GREEN);
    while(1) {
        PORTD |= (1 << RED);
        _delay_ms(1000);
        PORTD &= ~(1 << RED);
        PORTD |= (1 << GREEN);
        _delay_ms(1000);
        PORTD &= ~(1 << GREEN);
        PORTD |= (1 << BLUE);
        _delay_ms(1000);
        PORTD &= ~(1 << BLUE);
        // yellow
        PORTD |= (1 << RED) | (1 << GREEN);
        _delay_ms(1000);
        PORTD &= ~((1 << RED) | (1 << GREEN));
        // cyan
        PORTD |= (1 << BLUE) | (1 << GREEN);
        _delay_ms(1000);
        PORTD &= ~((1 << BLUE) | (1 << GREEN));
        // magenta
        PORTD |= (1 << BLUE) | (1 << RED);
        _delay_ms(1000);
        PORTD &= ~((1 << BLUE) | (1 << RED));
        // white
        PORTD |= (1 << BLUE) | (1 << GREEN) | (1 << RED);
        _delay_ms(1000);
        PORTD &= ~((1 << BLUE) | (1 << GREEN) | (1 << RED));
    }
}