#include "imu.h"
#include "i2c.h"
#include <stdint.h>
#include <util/delay.h>


#define LSM_WHO_AM_I      0x0F
#define LSM_CTRL1_XL      0x10
#define LSM_CTRL3_C       0x12
#define LSM_CTRL9_XL      0x18
#define LSM_STATUS        0x17
#define LSM_OUTX_L_XL     0x28


#define LSM6_WHO_AM_I_VAL 0x6C

#define LSM6_ACCEL_16G_LSB_mg   (0.488f)   

static uint8_t imu_addr = 0x00; 

int IMU_init(uint8_t addr7)
{
    imu_addr = addr7;
    I2C_init();
    _delay_ms(5);

    uint8_t who;
    if (I2C_readRegister(imu_addr, LSM_WHO_AM_I, &who) != 0)
        return -1;

    if (who != LSM6_WHO_AM_I_VAL)
        return -1;

    if (I2C_writeRegister(imu_addr, LSM_CTRL3_C, 0x44) != 0)
        return -1;


    if (I2C_writeRegister(imu_addr, LSM_CTRL1_XL, 0x4C) != 0)
        return -1;


    if (I2C_writeRegister(imu_addr, LSM_CTRL9_XL, 0x38) != 0)
        return -1;

    _delay_ms(20);
    return 0;
}

uint8_t IMU_getAddress(void)
{
    return imu_addr;
}

int IMU_checkNewData(void)
{
    uint8_t st = 0;

    if (I2C_readRegister(imu_addr, LSM_STATUS, &st) != 0)
        return 0;

    return (st & 0x01) ? 1 : 0;
}

int IMU_readAccBytes(uint8_t buf[6])
{
    if (!buf) return -1;

    if (I2C_readMulti(imu_addr, LSM_OUTX_L_XL, buf, 6) != 0)
        return -1;

    return 0;
}

int IMU_readAccRaw(int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t b[6];

    if (IMU_readAccBytes(b) < 0)
        return -1;

    *ax = (int16_t)((b[1] << 8) | b[0]);
    *ay = (int16_t)((b[3] << 8) | b[2]);
    *az = (int16_t)((b[5] << 8) | b[4]);

    return 0;
}

int IMU_readAcc_mg(float *ax_mg, float *ay_mg, float *az_mg)
{
    int16_t rx, ry, rz;

    if (IMU_readAccRaw(&rx, &ry, &rz) < 0)
        return -1;

    if (ax_mg) *ax_mg = rx * LSM6_ACCEL_16G_LSB_mg;
    if (ay_mg) *ay_mg = ry * LSM6_ACCEL_16G_LSB_mg;
    if (az_mg) *az_mg = rz * LSM6_ACCEL_16G_LSB_mg;

    return 0;
}
