#include <avr/io.h>

// page references: 91, 103, 125, 121, 131, 140.

int main() {
    DDRB |= (1 << PB1); // PB1 (OC1A) as output

    OCR1A = 31249; // Toggle for 0.5s, LED blink 1Hz
    // (16MHz / 256 * 0.5) - 1

    TCCR1A = (1 << COM1A0); // Toggle OC1A automatically compare match
    TCCR1B = (1 << WGM12) | (1 << CS12); // Mode CTC + prescaler 256

    while (1) {} // Loop empty, hardware does the job
}