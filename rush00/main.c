#include "i2c.h"

void led_init(void) { DDRB  |=  (1 << LED_PIN); }
void led_on(void)   { PORTB |=  (1 << LED_PIN); }
void led_off(void)  { PORTB &= ~(1 << LED_PIN); }

static role_t read_role(void) {
    return (PIND & (1 << PD2)) ? SLAVE : MASTER;
}

static void blink_wait(void) {
    led_on();
    _delay_ms(100);
    led_off();
    _delay_ms(100);
}

// Debounce for read button
static uint8_t read_button_stable(uint8_t pin) {
    uint8_t state = (PIND & (1 << pin)) ? 1 : 0;
    _delay_ms(20);
    return ((PIND & (1 << pin)) ? 1 : 0) == state ? state : 255; // 255 = unstable
}

// wait for button be pressed (LOW) and return role
static role_t wait_for_role_selection(void) {
    while (1) {
        // blink to sinalize"waiting"
        blink_wait();

        //(MASTER)
        if (read_button_stable(PD2) == 0) {
            _delay_ms(300); // aguarda soltar
            while (read_button_stable(PD2) == 0);
            _delay_ms(200);
            led_on();
            _delay_ms(500);
            led_off();
            return MASTER;
        }

        //(SLAVE)
        if (read_button_stable(PD4) == 0) {
            _delay_ms(300); // aguarda soltar
            while (read_button_stable(PD4) == 0);
            _delay_ms(200);
            led_on();
            _delay_ms(1000);
            led_off();
            return SLAVE;
        }
    }
}

int main(void) {
    led_init();

    // config pin as inputs with pull-up
    DDRD &= ~(1 << PD2); // PD2 input
    DDRD &= ~(1 << PD4); // PD4 input
    PORTD |= (1 << PD2); // pull-up
    PORTD |= (1 << PD4); // pull-up

    role_t role = wait_for_role_selection();

    // initialize I2C
    if (role == MASTER)
        i2c_init_master();
    else
        i2c_init_slave(SLAVE_ADDR);

    while (1) {
        if (role == MASTER)
            run_master();
        else
            run_slave();
    }
}
