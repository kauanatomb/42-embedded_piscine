#include <avr/io.h>
#include <util/delay.h>

int main() {
    // config out
    DDRB |= (1 << PB0);
    //config in
    DDRD &= ~(1 << PD2);
    // pull up
    PORTD |= (1 << PD2);
    uint8_t last_state = 1;
    while(1) {
        uint8_t current_state = (PIND & (1 << PD2)) != 0;
        if (last_state == 1 && current_state == 0) {
            _delay_ms(20);
            if ((PIND & (1 << PD2)) == 0)
                PORTB ^= (1 << PB0);
        }
        last_state = current_state;
    }

}

int 