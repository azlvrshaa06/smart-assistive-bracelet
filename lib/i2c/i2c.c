#include "i2c.h"
#include <avr/io.h>
#include <util/delay.h>

#define F_CPU 16000000UL

void i2c_init(uint32_t scl_hz)
{
    TWSR = 0x00; // prescaler = 1
    uint32_t twbr = ((F_CPU / scl_hz) - 16) / 2;
    if (twbr > 255) twbr = 255;
    TWBR = (uint8_t)twbr;
    TWCR = (1 << TWEN);
}

uint8_t i2c_start(uint8_t address)
{
    TWCR = (1<<TWINT)|(1<<TWSTA)|(1<<TWEN);
    
    uint16_t timeout = 10000;
    
    while (!(TWCR & (1<<TWINT))) {
    if (--timeout == 0) return I2C_TIMEOUT;
    }


    TWDR = address;
    TWCR = (1<<TWINT)|(1<<TWEN);
    while (!(TWCR & (1<<TWINT))) {
    if (--timeout == 0) return I2C_TIMEOUT;
    }

    return (TWSR & 0xF8);
}

uint8_t i2c_write(uint8_t data)
{
    TWDR = data;
    TWCR = (1<<TWINT)|(1<<TWEN);
   
    uint16_t timeout = 10000;
    
    while (!(TWCR & (1<<TWINT))) {
    if (--timeout == 0) return I2C_TIMEOUT;
    }
    return (TWSR & 0xF8);
}

uint8_t i2c_read_ack(void)
{
    TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWEA);
    
    uint16_t timeout = 10000;
    
    while (!(TWCR & (1<<TWINT))) {
    if (--timeout == 0) return I2C_TIMEOUT;
    }
    return TWDR;
}

uint8_t i2c_read_nack(void)
{
    TWCR = (1<<TWINT)|(1<<TWEN);
    uint16_t timeout = 10000;
    
    while (!(TWCR & (1<<TWINT))) {
    if (--timeout == 0) return I2C_TIMEOUT;
    }
    return TWDR;
}

void i2c_stop(void)
{
    TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWSTO);
}

void i2c_bus_recover(void)
{
    DDRC &= ~(1<<PC4);
    PORTC |= (1<<PC4);

    for (uint8_t i = 0; i < 9; i++) {
        DDRC |= (1<<PC5);
        PORTC &= ~(1<<PC5);
        _delay_us(5);
        DDRC &= ~(1<<PC5);
        PORTC |= (1<<PC5);
        _delay_us(5);
    }
    TWCR = (1<<TWINT)|(1<<TWEN)|(1<<TWSTO);
}
