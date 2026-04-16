#include <avr/io.h>
#include <util/delay.h>

int main() {
    DDRB |= (1 << PB1);
    DDRD &= ~(1 << PD2);
    DDRD &= ~(1 << PD4);
    PORTD |= (1 << PD2);
    PORTD |= (1 << PD4);

    uint8_t last_sw1 = 1;
    uint8_t last_sw2 = 1;

    /* TOP = 62499 (frequency 1Hz)
    ** F_CPU / Prescaler / Frequency - 1
    ** 16.000.000 / 256 / 1 - 1 = 62499 */
    ICR1 = 62499;

    /* Duty cycle 10% TOP
    ** 62499 * 0.10 = 6249 */
    OCR1A = 6249;
    uint8_t duty_cycle = 10;

    /* TCCR1A:
    ** COM1A1=1, COM1A0=0 -> Clear OC1A no compare match, Set no BOTTOM
    ** WGM11=1,  WGM10=0 -> Fast PWM low part (modo 14) */
    TCCR1A = (1 << COM1A1) | (1 << WGM11);

    /* TCCR1B:
    ** WGM13=1, WGM12=1 -> Fast PWM TOP=ICR1 high part (modo 14)
    ** CS12=1 -> Prescaler 256 — inicialize the timer */
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS12);

    while (1) {
        uint8_t sw1 = (PIND & (1 << PD2)) != 0;
        uint8_t sw2 = (PIND & (1 << PD4)) != 0;
        if (last_sw1 && !sw1) {
            _delay_ms(20);
            if (!(PIND & (1 << PD2))) {
                if (duty_cycle < 100) {
                    duty_cycle += 10;
                    OCR1A = (uint32_t)ICR1 * duty_cycle / 100;
                }
            }
        }
        if (last_sw2 && !sw2) {
            _delay_ms(20);
            if (!(PIND & (1 << PD4))) {
                if (!(duty_cycle == 10)) {
                    duty_cycle -= 10;
                    OCR1A = (uint32_t)ICR1 * duty_cycle / 100;
                }
            }
        }
        last_sw1 = sw1;
        last_sw2 = sw2;
    }
}