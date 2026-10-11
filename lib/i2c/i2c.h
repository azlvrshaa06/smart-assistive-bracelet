#ifndef I2C_H
#define I2C_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define I2C_OK 0
#define I2C_TIMEOUT 1
#define I2C_NACK 2
#define I2C_ERROR 3

void i2c_init(uint32_t scl_hz);

uint8_t i2c_start(uint8_t address);
uint8_t i2c_write(uint8_t data);

uint8_t i2c_read_ack(void);
uint8_t i2c_read_nack(void);

void i2c_stop(void);

void i2c_bus_recover(void);

#ifdef __cplusplus
}
#endif

#endif
