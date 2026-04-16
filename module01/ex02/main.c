#include <avr/io.h>

// page: 121, 122, 132, 140, 142

int main(void)
{
    /* PB1 (OC1A) as output */
    DDRB |= (1 << PB1);

    /* TOP = 62499 (frequency 1Hz)
    ** F_CPU / Prescaler / Frequency - 1
    ** 16.000.000 / 256 / 1 - 1 = 62499 */
    ICR1 = 62499;

    /* Duty cycle 10% TOP
    ** 62499 * 0.10 = 6249 */
    OCR1A = 6249;

    /* TCCR1A:
    ** COM1A1=1, COM1A0=0 -> Clear OC1A no compare match, Set no BOTTOM
    ** WGM11=1,  WGM10=0 -> Fast PWM low part (modo 14) */
    TCCR1A = (1 << COM1A1) | (1 << WGM11);

    /* TCCR1B:
    ** WGM13=1, WGM12=1 -> Fast PWM TOP=ICR1 high part (modo 14)
    ** CS12=1 -> Prescaler 256 — inicialize the timer */
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS12);

    while (1) {}
}