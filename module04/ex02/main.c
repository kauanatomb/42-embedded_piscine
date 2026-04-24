#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

volatile int8_t value = 0;
volatile uint8_t last_PIND;

// Vector 6 in the datasheet corresponds to __vector_5
// (vector numbering starts at RESET = 1 internally for GCC names)

void display(int8_t value) {
    uint8_t v = (uint8_t)value & 0x0F;
    uint8_t leds = (v & 0x07) | ((v & 0x08) << 1);
    PORTB = (PORTB & ~((1 << PB0) | (1 << PB1) | (1 << PB2) | (1 << PB4))) | leds;
}

void __vector_5(void) __attribute__((signal, used));
void __vector_5(void)
{
    uint8_t current = PIND;
    uint8_t changed = current ^ last_PIND;

    if ((changed & (1 << PD2)) && !(current & (1 << PD2))) {
        _delay_ms(30);

        if (!(PIND & (1 << PD2))) {
            value++;
            display(value);
        }
    }

    if ((changed & (1 << PD4)) && !(current & (1 << PD4))) {
        _delay_ms(30);

        if (!(PIND & (1 << PD4))) {
            value--;
            display(value);
        }
    }

    last_PIND = current;
}

void interrupts_init(void)
{
    // Pin Change Interrupt Control Register (p.82)
    PCICR |= (1 << PCIE2);
    // Pin Change Mask Register 2 (p.83)
    PCMSK2 |= (1 << PCINT18); // PD2
    PCMSK2 |= (1 << PCINT20); // PD4

    last_PIND = PIND;

    SREG |= (1 << 7);
}

void registers_init() {
    DDRB |= (1 << PB0);
    DDRB |= (1 << PB1);
    DDRB |= (1 << PB2);
    DDRB |= (1 << PB4);

    DDRD &= ~(1 << PD2);
    DDRD &= ~(1 << PD4);
    PORTD |= (1 << PD2);
    PORTD |= (1 << PD4);
    display(value);
}

int main() {
    registers_init();
    interrupts_init();

    while (1) {}
}