#include "i2c.h"

void run_master(void) {
    if (i2c_start(SLAVE_ADDR << 1 | TW_WRITE) == 0) {
        i2c_write(MSG_PING);
        i2c_stop();
    }
    _delay_ms(1000);
}

void run_slave(void) {
    uint8_t data = 0;
    uint8_t status = i2c_slave_listen(&data);

    if (status == TW_SR_DATA_ACK && data == MSG_PING) {
        led_on();
        _delay_ms(200);
        led_off();
    }
}

// wait TWI finishs (p. 225)
static void twi_wait(void) {
    while (!(TWCR & (1 << TWINT)));
}

void i2c_init_master(void) {
    TWBR = TWBR_VAL;
    TWSR = (0 << TWPS1) | (0 << TWPS0); // prescaler = 1 (p.222)
}

void i2c_init_slave(uint8_t addr) {
    TWAR = addr << 1; // slave address (bit 0 = TWGCE, let 0) (p.235)
    TWCR = (1 << TWEN) // enable TWI
         | (1 << TWEA) // enable ACK
         | (1 << TWINT); // clean flag
}

// rewturn 0 if START+address OK, 1 if fail (loose arbitragem or NACK) (p.223, 224, 229)
// helper for TWI state machine https://www.nongnu.org/avr-libc/user-manual/group__util__twi.html#ga8d3aca0acc182f459a51797321728168
uint8_t i2c_start(uint8_t addr_rw) {
    // send START
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    twi_wait();

    uint8_t status = TW_STATUS; // status comes from TWSR (events)
    if (status != TW_START && status != TW_REP_START)
        return 1;

    // send address + R/W
    TWDR = addr_rw;
    TWCR = (1 << TWINT) | (1 << TWEN);
    twi_wait();

    status = TW_STATUS;
    if (status != TW_MT_SLA_ACK && status != TW_MR_SLA_ACK)
        return 1;

    return 0;
}

// transmit stop (p.225)
void i2c_stop(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

// (p.224)
void i2c_write(uint8_t data) {
    // load data packet
    TWDR = data;
    // setup to transmit the data in TWDR
    TWCR = (1 << TWINT) | (1 << TWEN);
    twi_wait();
}

// TWEN set to enable TWI, TWEA set to acknoledge (p.232)
uint8_t i2c_read_ack(void) {
    TWCR = (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
    twi_wait();
    return TWDR;
}

// (p. 235)
uint8_t i2c_read_nack(void) {
    TWCR = (1 << TWINT) | (1 << TWEN);
    twi_wait();
    return TWDR;
}

// Slave keep data. return status TWI, save data in *data (p. 224)
uint8_t i2c_slave_listen(uint8_t *data) {
    TWCR = (1 << TWEN) | (1 << TWEA) | (1 << TWINT);

    while (1) {
        twi_wait();
        uint8_t status = TW_STATUS;

        if (status == TW_SR_SLA_ACK) {
            // address received -> continue
            TWCR = (1 << TWEN) | (1 << TWEA) | (1 << TWINT);
        }
        else if (status == TW_SR_DATA_ACK) {
            *data = TWDR;
            return status;
        }
        else if (status == TW_SR_STOP) {
            TWCR = (1 << TWEN) | (1 << TWEA) | (1 << TWINT);
        }
        else {
            return status;
        }
    }
}