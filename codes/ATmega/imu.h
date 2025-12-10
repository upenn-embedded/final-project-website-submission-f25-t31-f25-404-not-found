#ifndef IMU_H
#define IMU_H

#include <stdint.h>


int IMU_init(uint8_t addr7);

int IMU_checkNewData(void);

int IMU_readAccRaw(int16_t *ax, int16_t *ay, int16_t *az);

int IMU_readAcc_mg(float *ax_mg, float *ay_mg, float *az_mg);

int IMU_readAccBytes(uint8_t buf[6]);

uint8_t IMU_getAddress(void);

#define IMU_ACCEL_LSB_mg 0.061f

#endif /* IMU_H */
