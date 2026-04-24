#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

volatile uint8_t button_pressed = 0;

// each event on hardware has its own vector number
// 2 -1 -> INT0 External Interrupt Request 0 (p.74)
void __vector_1(void) __attribute__((signal, used));
void __vector_1(void) {
    /* disable INT0 for the moment: to avoid multi interruption
        caused by the bounce of button*/
    EIMSK &= ~(1 << INT0);
    button_pressed = 1;
}

int main() {
    // config out
    DDRB |= (1 << PB0);
    //config in
    DDRD &= ~(1 << PD2);
    // pull up
    PORTD |= (1 << PD2);

    EICRA = (EICRA & ~((1 << ISC01) | (1 << ISC00))) | (1 << ISC01); // falling edge (p.80)
    EIFR |= (1 << INTF0); // NTF0: External Interrupt Flag 0 (p.81)
    EIMSK |= (1 << INT0); // INT0: External Interrupt Request 0 Enable (p.81)
    SREG |= (1 << 7);

    while (1) {
        if (button_pressed) {
            _delay_ms(190);
            PORTB ^= (1 << PB0);
            button_pressed = 0;
            EIFR  |= (1 << INTF0); // clean flag interruption
            EIMSK |= (1 << INT0); // enable
        }
    }
}