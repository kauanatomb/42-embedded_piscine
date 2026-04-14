#include <avr/io.h>

int main() {
    // config out
    DDRB |= (1 << PB0);
    //config in
    DDRD &= ~(1 << PD2);
    // pull up
    PORTD |= (1 << PD2);
    while(1) {
        if (PIND & (1 << PD2))
            PORTB &= ~(1 << PB0);
        else
            PORTB |= (1 << PB0);
    }

}