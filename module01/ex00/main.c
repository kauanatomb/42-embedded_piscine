#include <avr/io.h>
// page 85
// Loop delay: each iteration cost 25 cycles
// 16.000.000 / 25 = 640.000 iteration per second
// to 500ms: 640.000 * 0.5 = 320.000

void delay_500ms(void) {
    volatile unsigned long i;
    i = 320000UL;
    while (i--)
        ;
}

int main(void) {
    // DDRB: set bit 1 on PB1 as OUT
    DDRB |= (1 << PB1);

    while (1) {
        // PIN: write 1 no bit and execute toggle
        PINB |= (1 << PB1);
        delay_500ms();
    }
}