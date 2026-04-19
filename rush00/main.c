#include "i2c.h"

void led_init(void) { DDRB  |=  (1 << LED_PIN); }
void led_on(void)   { PORTB |=  (1 << LED_PIN); }
void led_off(void)  { PORTB &= ~(1 << LED_PIN); }

// Debounce for read button
static uint8_t read_button_stable(uint8_t pin) {
    uint8_t state = (PIND & (1 << pin)) ? 1 : 0;
    _delay_ms(20);
    return ((PIND & (1 << pin)) ? 1 : 0);
}

// Check if button is pressed (stable LOW)
static uint8_t button_pressed(uint8_t pin) {
    return read_button_stable(pin) == 0;
}

int main(void) {
    led_init();
    DDRD  &= ~(1 << PD2);
    PORTD |=  (1 << PD2);

    // all are slaves
    i2c_init_slave(SLAVE_ADDR);

    // Fase negociation only moment can become master
    while (get_current_role() == SLAVE) {
        uint8_t data = 0;

        // waiting for inicialization
        led_on();
        _delay_ms(100);
        led_off();
        // if already received slave confirmation, ignore button
        if (i2c_slave_poll(&data) && data == MSG_YOU_ARE_SLAVE) {
            break; // role defined as SLAVE, go out of negociation
        }

        // Just try to be master if no slave is confirmed
        if (button_pressed(PD2)) {
            switch_to_master();
        }
    }

    // Roles defined loop game
    while (1) {
        if (get_current_role() == MASTER)
            run_master();
        else
            run_slave();
    }
}