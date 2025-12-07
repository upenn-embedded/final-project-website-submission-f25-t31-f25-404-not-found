#ifndef IMU_H
#define IMU_H

#include <stdint.h>

/* LSM6DS0/LSM6DSO-friendly API */

/* Initialize IMU driver with 7-bit I2C address (e.g., 0x6B).
 * Returns 0 on success, -1 on failure (WHO_AM_I mismatch).
 */
int IMU_init(uint8_t addr7);

/* Check if new accelerometer data available (non-zero if new).
 * Returns 1 if new accel data ready, 0 otherwise.
 */
int IMU_checkNewData(void);

/* Read raw accelerometer samples (signed 16-bit) for X/Y/Z.
 * Returns 0 on success, -1 on failure.
 */
int IMU_readAccRaw(int16_t *ax, int16_t *ay, int16_t *az);

/* Read accelerometer in milli-g (mg). Example: 1000 mg = 1 g.
 * Returns 0 on success, -1 on failure.
 */
int IMU_readAcc_mg(float *ax_mg, float *ay_mg, float *az_mg);

/* Low-level read all 6 accel bytes into provided buffer (6 bytes) */
int IMU_readAccBytes(uint8_t buf[6]);

/* Device address accessor (7-bit) */
uint8_t IMU_getAddress(void);

/* Scale constant description:
 * For ±2g FS on LSM6DS0/LSM6DSO:
 *   1 LSB = 0.061 mg (i.e., raw * 0.061 = mg)
 */
#define IMU_ACCEL_LSB_mg 0.061f

#endif /* IMU_H */
