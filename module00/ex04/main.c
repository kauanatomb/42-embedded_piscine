#include <avr/io.h>
#include <util/delay.h>

void display(int8_t value) {
    uint8_t v = (uint8_t)value & 0x0F;
    uint8_t leds = (v & 0x07) | ((v & 0x08) << 1);
    PORTB = (PORTB & ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4))) | leds;
}

int main() {
    DDRB |= (1 << PB0);
    DDRB |= (1 << PB1);
    DDRB |= (1 << PB2);
    DDRB |= (1 << PB4);

    DDRD &= ~(1 << PD2);
    DDRD &= ~(1 << PD4);
    PORTD |= (1 << PD2);
    PORTD |= (1 << PD4);

    uint8_t last_sw1 = 1;
    uint8_t last_sw2 = 1;
    int8_t value = 0;
    display(value);
    while (1) {
        uint8_t sw1 = (PIND & (1 << PD2)) != 0;
        uint8_t sw2 = (PIND & (1 << PD4)) != 0;
        if (last_sw1 && !sw1) {
            _delay_ms(20);
            if (!(PIND & (1 << PD2))) {
                value++;
                display(value);
            }
        }
        if (last_sw2 && !sw2) {
            _delay_ms(20);
            if (!(PIND & (1 << PD4))) {
                value--;
                display(value);
            }
        }
        last_sw1 = sw1;
        last_sw2 = sw2;
    }
}