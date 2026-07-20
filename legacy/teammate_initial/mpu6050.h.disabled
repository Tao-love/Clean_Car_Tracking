#ifndef MPU6050_H
#define MPU6050_H

#include <stdbool.h>
#include <stdint.h>

#define MPU6050_CALIBRATION_SAMPLES (500U)

bool MPU6050_Init(void);
bool MPU6050_CalibrateGyroZ(uint16_t samples);
bool MPU6050_ReadGyroZRaw(int16_t *gyroZRaw);
bool MPU6050_ReadGyroZ(int16_t *gyroZ);
int16_t MPU6050_GetGyroZOffset(void);
bool MPU6050_IsReady(void);

#endif /* MPU6050_H */
