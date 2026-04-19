#ifndef I2C_H
#define I2C_H

#include <avr/io.h>
#include <util/delay.h>
#include <util/twi.h>

#define F_CPU 16000000UL
#define I2C_FREQ 100000UL
// Slave address (p.217)
#define SLAVE_ADDR 0x42
#define LED_PIN PB0
// test proporse
#define MSG_PING 0x01
#define MSG_YOU_ARE_SLAVE 0x02

// TWBR = (F_CPU / I2C_FREQ - 16) / 2  -> (16M/100k - 16) / 2 = 72
#define TWBR_VAL 72

typedef enum { SLAVE = 0, MASTER = 1 } role_t;
typedef enum { START_GAME = 0 } game_t;
extern role_t current_role;

// LED  and buttons functions
void led_init(void);
void led_on(void);
void led_off(void);
uint8_t button_pressed(uint8_t pin);

// I2C functions
uint8_t i2c_start(uint8_t addr_rw); // return 0 if OK
void i2c_stop(void);
void i2c_write(uint8_t data);
uint8_t i2c_read_ack(void);
uint8_t i2c_read_nack(void);
uint8_t i2c_slave_listen(uint8_t *data); // return status
void twi_wait(void);

// Main functions
void run_master(void);
void run_slave(void);

// Role management
void i2c_init_master(void);
void i2c_init_slave(uint8_t addr);
void switch_to_master(void);
role_t get_current_role(void);
void run_master_signal(void);
uint8_t i2c_slave_poll(uint8_t *data);

// game
void countdown(void);
void start_game_slave(void);
void start_game_master(void);

#endif