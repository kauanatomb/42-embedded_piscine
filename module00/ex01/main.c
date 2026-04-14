#include <avr/io.h>

int main() {
    // DDRB = Data Direction Register for Port B
    // PB0 = 1 to configure PB0 as OUTPUT
    DDRB |= (1 << PB0);
    // PORTB control output value on PB0
    // PB0 = 1 to put level HIGH on PB0 (LED works)
    PORTB |= (1 << PB0);

    while(1) {
        // to keep state
    }
}