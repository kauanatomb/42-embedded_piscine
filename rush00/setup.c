#include "i2c.h"
role_t current_role = SLAVE;

void i2c_init_slave(uint8_t addr) {
    TWAR = addr << 1; // slave address (bit 0 = TWGCE, let 0) (p.235)
    TWCR = (1 << TWEN) // enable TWI
         | (1 << TWEA) // enable ACK
         | (1 << TWINT); // clean flag
}

void i2c_init_master(void) {
    TWCR = 0; // desactivate TWI completly
    TWAR = 0; // clean slave address
    TWBR = TWBR_VAL;
    TWSR = 0; // prescaler = 1
    TWCR = (1 << TWEN);
}

void run_master_signal(void) {
    if (i2c_start(SLAVE_ADDR << 1 | TW_WRITE) == 0) {
        i2c_write(MSG_YOU_ARE_SLAVE);
        i2c_stop();
    }
    _delay_ms(1000);
}

// Switch device to master role and notify other device
void switch_to_master(void) {
    current_role = MASTER;

    // Initialize as master
    i2c_init_master();

    // Signal to other device that we are now master (so it stays slave)
    run_master_signal();
}

// Return 1 and save the data, 0 if nothing arrived
uint8_t i2c_slave_poll(uint8_t *data) {
    // Check if TWINT is set (event TWI pending)
    if (!(TWCR & (1 << TWINT)))
        return 0;

    uint8_t status = TW_STATUS;

    if (status == TW_SR_SLA_ACK) {
        TWCR = (1 << TWEN) | (1 << TWEA) | (1 << TWINT);
        return 0;
    }
    if (status == TW_SR_DATA_ACK) {
        *data = TWDR;
        TWCR = (1 << TWEN) | (1 << TWEA) | (1 << TWINT);
        return 1;
    }
    if (status == TW_SR_STOP) {
        TWCR = (1 << TWEN) | (1 << TWEA) | (1 << TWINT);
    }
    return 0;
}