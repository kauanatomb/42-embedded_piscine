#include "i2c.h"

// master: poll until both are ready
void start_game_master(void) {
    uint8_t master_ready = 0;
    uint8_t slave_ready = 0;

    while (!master_ready || !slave_ready) {
        // verify local button
        if (!master_ready && button_pressed(PD2)) {
            _delay_ms(300);
            while (button_pressed(PD2));
            master_ready = 1;
        }

        // poll slave
        if (i2c_start(SLAVE_ADDR << 1 | TW_WRITE) == 0) {
            i2c_write(MSG_POLL);
            i2c_stop();
        }
        if (i2c_start(SLAVE_ADDR << 1 | TW_READ) == 0) {
            uint8_t response = i2c_read_nack();
            i2c_stop();
            if (response == MSG_READY)
                slave_ready = 1;
        }
        _delay_ms(50);
    }

    // both ready
    if (i2c_start(SLAVE_ADDR << 1 | TW_WRITE) == 0) {
        i2c_write(MSG_START);
        i2c_stop();
    }
}

// (p. 225)
// slave reply to poll master
void start_game_slave(void) {
    uint8_t ready = 0;

    while (1) {
        uint8_t data = 0;
        // wait master ask
        i2c_slave_listen(&data);

        if (data == MSG_POLL) {
            // master wants to know if slave is ready
            // prepare answer
            TWDR = ready ? MSG_READY : MSG_NOT_READY;
            TWCR = (1 << TWEN) | (1 << TWEA) | (1 << TWINT);
            twi_wait();
            if ((TWSR & 0xF8) == TW_ST_SLA_ACK) {
                TWCR = (1 << TWINT) | (1 << TWEN);
                twi_wait();
            }
        }
        if (data == MSG_START)
            break; // master confirmed, go countdown
        if (button_pressed(PD2))
            ready = 1; // set ready but stay in the loop
    }
}

uint8_t countdown(void) {
    for(int i = 6; i > 0; i--) {
        led_on();
        _delay_ms(200);
        led_off();
        _delay_ms(200);
        led_on();
        _delay_ms(200);
        led_off();
        if (get_current_role() == MASTER) {
            // if master pressed his button here signalize to slave to stop countdown
            // listen to slave if he presses his button and stop countdown
            return 1;
        }
    }
    return 0;
}