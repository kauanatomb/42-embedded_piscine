#include "i2c.h"

void start_game_master(void) {
    while (!button_pressed(PD2));
    if (i2c_start(SLAVE_ADDR << 1 | TW_WRITE) == 0) {
        i2c_write(START_GAME);
        i2c_stop();
    }
    uint8_t status = 0;
    // read
    if (i2c_start(SLAVE_ADDR << 1 | TW_READ) == 0) {
        status = i2c_read_nack();
        i2c_stop();
    }
    if (status == START_GAME)
        countdown();
}

// (p. 225)
void start_game_slave(void) {
    // press the button
    while(!button_pressed(PD2));
    // i should write a message to master
    while (1) {
        if ((TWSR & 0xF8) == TW_ST_SLA_ACK) {
            TWDR = START_GAME;
            TWCR = (1<<TWINT)|(1<<TWEA)|(1<<TWEN)|(1<<TWIE);
            twi_wait();
            led_on();
            _delay_ms(1000);
            led_off();
            break;
        }
    }
    countdown();
}

void countdown(void) {
    for(int i = 6; i > 0; i--) {
        led_on();
        _delay_ms(1000);
        led_off();
    }
}