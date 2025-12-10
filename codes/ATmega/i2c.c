
#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include "i2c.h"


void I2C_init(void)
{
    TWSR0 = 0x00;   
    TWBR0 = 72;     

}


static uint8_t i2c_start_send_addr(uint8_t addr_rw)
{
    TWCR0 = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
    while (!(TWCR0 & (1<<TWINT)));

    
    TWDR0 = addr_rw;
    TWCR0 = (1<<TWINT) | (1<<TWEN);
    while (!(TWCR0 & (1<<TWINT)));

    return (TWSR0 & 0xF8);
}

static void i2c_stop(void)
{
    TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
}

int I2C_writeRegister(uint8_t addr7, uint8_t reg, uint8_t data)
{
    uint8_t status;

    status = i2c_start_send_addr((addr7 << 1) | 0); 
    if (status != 0x18) { 
        i2c_stop();
        return -1;
    }

    TWDR0 = reg;
    TWCR0 = (1<<TWINT) | (1<<TWEN);
    while (!(TWCR0 & (1<<TWINT)));
    if ((TWSR0 & 0xF8) != 0x28) { 
        i2c_stop();
        return -1;
    }

    TWDR0 = data;
    TWCR0 = (1<<TWINT) | (1<<TWEN);
    while (!(TWCR0 & (1<<TWINT)));
    if ((TWSR0 & 0xF8) != 0x28) {
        i2c_stop();
        return -1;
    }

    i2c_stop();
    return 0;
}

int I2C_readRegister(uint8_t addr7, uint8_t reg, uint8_t *data)
{
    uint8_t status;

    if (!data) return -1;

    status = i2c_start_send_addr((addr7 << 1) | 0); 
    if (status != 0x18) { i2c_stop(); return -1; }


    TWDR0 = reg;
    TWCR0 = (1<<TWINT) | (1<<TWEN);
    while (!(TWCR0 & (1<<TWINT)));
    if ((TWSR0 & 0xF8) != 0x28) { i2c_stop(); return -1; }

    status = i2c_start_send_addr((addr7 << 1) | 1);
    if (status != 0x40) { i2c_stop(); return -1; } 

    TWCR0 = (1<<TWINT) | (1<<TWEN); 
    while (!(TWCR0 & (1<<TWINT)));
    *data = TWDR0;

    i2c_stop();
    return 0;
}

int I2C_readMulti(uint8_t addr7, uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t status;
    if (!buf || len == 0) return -1;

    status = i2c_start_send_addr((addr7 << 1) | 0); 
    if (status != 0x18) { i2c_stop(); return -1; }

    TWDR0 = reg;
    TWCR0 = (1<<TWINT) | (1<<TWEN);
    while (!(TWCR0 & (1<<TWINT)));
    if ((TWSR0 & 0xF8) != 0x28) { i2c_stop(); return -1; }

    status = i2c_start_send_addr((addr7 << 1) | 1); 
    if (status != 0x40) { i2c_stop(); return -1; }

    for (uint8_t i = 0; i < len; ++i) {
        if (i < (len - 1)) {
            TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWEA);
        } else {

            TWCR0 = (1<<TWINT) | (1<<TWEN);
        }
        while (!(TWCR0 & (1<<TWINT)));
        buf[i] = TWDR0;
    }

    i2c_stop();
    return 0;
}
