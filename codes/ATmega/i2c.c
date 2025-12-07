/* i2c.c - TWI/I2C master for AVR (classic TWI registers)
 *
 * Exposes:
 *   I2C_init()
 *   I2C_writeRegister(addr7, reg, data)
 *   I2C_readRegister(addr7, reg, &data)
 *   I2C_readMulti(addr7, reg, buf, len)
 *
 * All addresses are 7-bit (0x00..0x7F). Functions return 0 on success,
 * -1 on failure.
 */

/* Define CPU frequency BEFORE util/delay if you use _delay_xxx anywhere */
#ifndef F_CPU
#define F_CPU 16000000UL
#endif
#include <stdio.h>
#include <avr/io.h>
#include <util/delay.h>
#include "i2c.h"

/* Set TWI bit rate for 100kHz at 16 MHz:
 * SCL = F_CPU / (16 + 2*TWBR*prescaler)
 * With prescaler = 1 (TWSR prescaler bits = 0), TWBR = 72 -> ~100kHz
 */
void I2C_init(void)
{
    TWSR0 = 0x00;   /* prescaler = 1 */
    TWBR0 = 72;     /* TWBR value for ~100kHz @ 16MHz */
    /* Optionally: enable pull-ups on SDA/SCL pins here if needed */
}

/* Start and send SLA+R/W byte.
 * addr_rw = (addr7 << 1) | R/Wbit
 * Returns TWI status code (TWSR & 0xF8) or 0xFF on bus error.
 */
static uint8_t i2c_start_send_addr(uint8_t addr_rw)
{
    /* Send START */
    TWCR0 = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
    while (!(TWCR0 & (1<<TWINT)));

    /* Check for start transmitted: 0x08 or repeated start 0x10 are typical start codes.
       We'll continue and send address anyway. */
    /* Load address + R/W and transmit */
    TWDR0 = addr_rw;
    TWCR0 = (1<<TWINT) | (1<<TWEN);
    while (!(TWCR0 & (1<<TWINT)));

    return (TWSR0 & 0xF8);
}

static void i2c_stop(void)
{
    TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
    /* STOP is transmitted; no TWINT flag on STOP, just return */
}

/* Write one register (send reg then data) */
int I2C_writeRegister(uint8_t addr7, uint8_t reg, uint8_t data)
{
    uint8_t status;

    status = i2c_start_send_addr((addr7 << 1) | 0); /* SLA+W */
    if (status != 0x18) { /* 0x18 = SLA+W ACK */
        i2c_stop();
        return -1;
    }

    /* Send register index */
    TWDR0 = reg;
    TWCR0 = (1<<TWINT) | (1<<TWEN);
    while (!(TWCR0 & (1<<TWINT)));
    if ((TWSR0 & 0xF8) != 0x28) { /* 0x28 = data transmitted, ACK received */
        i2c_stop();
        return -1;
    }

    /* Send data */
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

/* Read single register (reg -> repeated start -> read 1 byte) */
int I2C_readRegister(uint8_t addr7, uint8_t reg, uint8_t *data)
{
    uint8_t status;

    if (!data) return -1;

    status = i2c_start_send_addr((addr7 << 1) | 0); /* SLA+W */
    if (status != 0x18) { i2c_stop(); return -1; }

    /* send register address */
    TWDR0 = reg;
    TWCR0 = (1<<TWINT) | (1<<TWEN);
    while (!(TWCR0 & (1<<TWINT)));
    if ((TWSR0 & 0xF8) != 0x28) { i2c_stop(); return -1; }

    /* Repeated start and SLA+R */
    status = i2c_start_send_addr((addr7 << 1) | 1); /* SLA+R */
    if (status != 0x40) { i2c_stop(); return -1; } /* 0x40 = SLA+R ACK */

    /* Read single byte and NACK */
    TWCR0 = (1<<TWINT) | (1<<TWEN); /* NACK on last byte */
    while (!(TWCR0 & (1<<TWINT)));
    *data = TWDR0;

    i2c_stop();
    return 0;
}

/* Multi-byte read (auto-increment register) */
int I2C_readMulti(uint8_t addr7, uint8_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t status;
    if (!buf || len == 0) return -1;

    status = i2c_start_send_addr((addr7 << 1) | 0); /* SLA+W */
    if (status != 0x18) { i2c_stop(); return -1; }

    /* send start register */
    TWDR0 = reg;
    TWCR0 = (1<<TWINT) | (1<<TWEN);
    while (!(TWCR0 & (1<<TWINT)));
    if ((TWSR0 & 0xF8) != 0x28) { i2c_stop(); return -1; }

    /* Repeated start and SLA+R */
    status = i2c_start_send_addr((addr7 << 1) | 1); /* SLA+R */
    if (status != 0x40) { i2c_stop(); return -1; }

    for (uint8_t i = 0; i < len; ++i) {
        if (i < (len - 1)) {
            /* ACK to receive more bytes */
            TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWEA);
        } else {
            /* last byte: NACK */
            TWCR0 = (1<<TWINT) | (1<<TWEN);
        }
        while (!(TWCR0 & (1<<TWINT)));
        buf[i] = TWDR0;
    }

    i2c_stop();
    return 0;
}
