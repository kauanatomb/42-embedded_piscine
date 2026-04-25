#include <avr/io.h>
#include <avr/interrupt.h>

volatile uint8_t debounce_ticks = 0;
volatile uint8_t debounce_active = 0;  // Flag: 1=processing, 0=idle
volatile uint8_t state = 0;            // 0=waiting failling, 1=waiting rising

// INT0: falling or rising
void __vector_1(void) __attribute__((signal, used));
void __vector_1(void) {
    if (!debounce_active) {
        EIMSK &= ~(1 << INT0);
        debounce_ticks = 5; // 80ms
        debounce_active = 1;
    }
}

// 15 - 1 TIMER0 COMPA Timer/Counter0 Compare Match A
void __vector_14(void) __attribute__((signal, used));
void __vector_14(void) {
    if (!debounce_active) return; // does nothing if debounce inactive
    
    if (debounce_ticks > 0) {
        debounce_ticks--;
        return;
    }

    if (state == 0) {
        // Falling edge confirmed
        PORTB ^= (1 << PB0);
        state = 1;
        EIFR |= (1 << INTF0);
        EIMSK &= ~(1 << INT0);
        // Config INT0 to rising edge
        EICRA = (EICRA & ~((1 << ISC01) | (1 << ISC00))) | ((1 << ISC01) | (1 << ISC00));
    } 
    else if (state == 1) {
        // Rising edge confirmed
        state = 0; // wait for falling edge
        EIFR |= (1 << INTF0); // clean flag
        EIMSK &= ~(1 << INT0); // to ignore interruptions
        // config INT0 to falling edge
        EICRA = (EICRA & ~((1 << ISC01) | (1 << ISC00))) | (1 << ISC01);
    }
    debounce_active = 0;  // desactivate debounce
    EIMSK |= (1 << INT0); // enable INT0
}

int main() {
    DDRB |= (1 << PB0);
    DDRD &= ~(1 << PD2);
    PORTD |= (1 << PD2);

    // INT0: falling edge initial
    EICRA = (1 << ISC01);
    EIFR |= (1 << INTF0);
    EIMSK |= (1 << INT0);

    // Timer0 - CTC mode
    TCCR0A = (1 << WGM01);
    TCCR0B = (1 << CS02);
    OCR0A = 124;
    TIMSK0 |= (1 << OCIE0A);

    SREG = (1 << 7);

    while (1) {}
}