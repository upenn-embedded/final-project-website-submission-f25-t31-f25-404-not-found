#ifndef I2C_H
#define I2C_H

#include <stdint.h>

/* Return: 0 = success, -1 = error (NACK or bus error) */
void I2C_init(void);
int I2C_writeRegister(uint8_t addr7, uint8_t reg, uint8_t data);
int I2C_readRegister(uint8_t addr7, uint8_t reg, uint8_t *data);
int I2C_readMulti(uint8_t addr7, uint8_t reg, uint8_t *buf, uint8_t len);

#endif /* I2C_H */
