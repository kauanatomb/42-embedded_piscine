#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

volatile uint8_t up = 1;
volatile uint16_t duty = 0;

// each event on hardware has its own vector number
// 15 -1 -> Timer/Counter0 Compare Match A(p.74)
void __vector_14(void) __attribute__((signal, used));
void __vector_14(void)
{
    if (up)
        duty++;
    else
        duty--;

    if (duty >= ICR1) {
        duty = ICR1;
        up = 0;
    }
    else if (duty == 0) {
        up = 1;
    }

    OCR1A = duty;
}

void timers_init(void)
{
    DDRB |= (1 << PB1);
    //timer1
    /* f_PWM = F_CPU / (prescaler * (1 + TOP))
    TOP = (16_000_000 / (64 * 1000)) - 1
    = 250 - 1
    ** 16.000.000 / (64 * 1 - 1000) = 62499 */
    ICR1 = 249; // high frequency

    /* Duty cycle will be increased by interruption */
    OCR1A = 0;

    /* TCCR1A:
    ** COM1A1=1, COM1A0=0 -> Clear OC1A no compare match, Set no BOTTOM
    ** WGM11=1,  WGM10=0 -> Fast PWM low part (modo 14) */
    TCCR1A = (1 << COM1A1) | (1 << WGM11);

    /* TCCR1B:
    ** WGM13=1, WGM12=1 -> Fast PWM TOP=ICR1 high part (modo 14)
    ** Prescaler 64 — inicialize the timer */
    TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11) | (1 << CS10);

    // timer0 - CTC mode
    TCCR0A = (1 << WGM01) | (1 << WGM00) | (1 << COM0B1) | (1 << COM0A1);
    TCCR0B = (1 << CS02); // prescaler 256
    TIMSK0 |= (1 << OCIE0A);
    OCR0A = 78;

    // Enable global interrupts (p.20)
    SREG |= (1 << 7);
}

int main() {
    timers_init();

    while (1) {}
}