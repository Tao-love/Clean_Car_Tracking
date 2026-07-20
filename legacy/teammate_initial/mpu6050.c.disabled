#include <stdbool.h>
#include <stdint.h>

#include "ti_msp_dl_config.h"
#include "mpu6050.h"

#define MPU6050_I2C_ADDR        (0x68U)
#define MPU6050_REG_SMPLRT_DIV  (0x19U)
#define MPU6050_REG_CONFIG      (0x1AU)
#define MPU6050_REG_GYRO_CONFIG (0x1BU)
#define MPU6050_REG_ACCEL_CONFIG (0x1CU)
#define MPU6050_REG_GYRO_ZOUT_H (0x47U)
#define MPU6050_REG_PWR_MGMT_1  (0x6BU)
#define MPU6050_REG_WHO_AM_I    (0x75U)
#define MPU6050_WHO_AM_I_VALUE  (0x68U)
#define MPU6050_I2C_TIMEOUT     (50000U)

static bool gMpu6050Ready = false;
static int16_t gGyroZOffset = 0;

static bool MPU6050_WriteReg(uint8_t reg, uint8_t value);
static bool MPU6050_ReadRegs(uint8_t reg, uint8_t *data, uint8_t len);
static bool MPU6050_WaitIdle(void);
static bool MPU6050_WaitNotBusy(void);
static bool MPU6050_CheckError(void);

bool MPU6050_Init(void)
{
    uint8_t whoAmI = 0;

    gMpu6050Ready = false;
    gGyroZOffset = 0;

    delay_cycles(CPUCLK_FREQ / 10U);

    if (!MPU6050_ReadRegs(MPU6050_REG_WHO_AM_I, &whoAmI, 1U)) {
        return false;
    }
    if (whoAmI != MPU6050_WHO_AM_I_VALUE) {
        return false;
    }

    if (!MPU6050_WriteReg(MPU6050_REG_PWR_MGMT_1, 0x00U)) {
        return false;
    }
    delay_cycles(CPUCLK_FREQ / 100U);

    if (!MPU6050_WriteReg(MPU6050_REG_SMPLRT_DIV, 0x07U)) {
        return false;
    }
    if (!MPU6050_WriteReg(MPU6050_REG_CONFIG, 0x03U)) {
        return false;
    }
    if (!MPU6050_WriteReg(MPU6050_REG_GYRO_CONFIG, 0x00U)) {
        return false;
    }
    if (!MPU6050_WriteReg(MPU6050_REG_ACCEL_CONFIG, 0x00U)) {
        return false;
    }

    gMpu6050Ready = true;
    return true;
}

bool MPU6050_CalibrateGyroZ(uint16_t samples)
{
    int16_t gyroZRaw = 0;
    int32_t sum = 0;
    uint16_t i;

    if (!gMpu6050Ready || samples == 0U) {
        return false;
    }

    for (i = 0; i < samples; i++) {
        if (!MPU6050_ReadGyroZRaw(&gyroZRaw)) {
            return false;
        }
        sum += gyroZRaw;
        delay_cycles(CPUCLK_FREQ / 1000U);
    }

    gGyroZOffset = (int16_t) (sum / (int32_t) samples);
    return true;
}

bool MPU6050_ReadGyroZRaw(int16_t *gyroZRaw)
{
    uint8_t data[2];

    if (gyroZRaw == 0) {
        return false;
    }
    if (!gMpu6050Ready) {
        return false;
    }
    if (!MPU6050_ReadRegs(MPU6050_REG_GYRO_ZOUT_H, data, 2U)) {
        return false;
    }

    *gyroZRaw = (int16_t) (((uint16_t) data[0] << 8) | data[1]);
    return true;
}

bool MPU6050_ReadGyroZ(int16_t *gyroZ)
{
    int16_t gyroZRaw = 0;

    if (gyroZ == 0) {
        return false;
    }
    if (!MPU6050_ReadGyroZRaw(&gyroZRaw)) {
        return false;
    }

    *gyroZ = (int16_t) (gyroZRaw - gGyroZOffset);
    return true;
}

int16_t MPU6050_GetGyroZOffset(void)
{
    return gGyroZOffset;
}

bool MPU6050_IsReady(void)
{
    return gMpu6050Ready;
}

static bool MPU6050_WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t data[2];

    data[0] = reg;
    data[1] = value;

    if (!MPU6050_WaitIdle()) {
        return false;
    }

    DL_I2C_fillControllerTXFIFO(I2C_MPU6050_INST, data, 2U);
    DL_I2C_startControllerTransfer(I2C_MPU6050_INST, MPU6050_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_TX, 2U);
    delay_cycles(100U);

    if (!MPU6050_WaitNotBusy()) {
        return false;
    }

    return MPU6050_CheckError();
}

static bool MPU6050_ReadRegs(uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t i;

    if (data == 0 || len == 0U) {
        return false;
    }
    if (!MPU6050_WaitIdle()) {
        return false;
    }

    DL_I2C_fillControllerTXFIFO(I2C_MPU6050_INST, &reg, 1U);
    DL_I2C_startControllerTransfer(I2C_MPU6050_INST, MPU6050_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_TX, 1U);
    delay_cycles(100U);

    if (!MPU6050_WaitNotBusy()) {
        return false;
    }
    if (!MPU6050_CheckError()) {
        return false;
    }
    if (!MPU6050_WaitIdle()) {
        return false;
    }

    DL_I2C_startControllerTransfer(I2C_MPU6050_INST, MPU6050_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_RX, len);

    for (i = 0; i < len; i++) {
        uint32_t timeout = MPU6050_I2C_TIMEOUT;

        while (DL_I2C_isControllerRXFIFOEmpty(I2C_MPU6050_INST)) {
            if (timeout-- == 0U) {
                return false;
            }
        }
        data[i] = DL_I2C_receiveControllerData(I2C_MPU6050_INST);
    }

    if (!MPU6050_WaitNotBusy()) {
        return false;
    }

    return MPU6050_CheckError();
}

static bool MPU6050_WaitIdle(void)
{
    uint32_t timeout = MPU6050_I2C_TIMEOUT;

    while ((DL_I2C_getControllerStatus(I2C_MPU6050_INST) &
            DL_I2C_CONTROLLER_STATUS_IDLE) == 0U) {
        if (timeout-- == 0U) {
            return false;
        }
    }

    return true;
}

static bool MPU6050_WaitNotBusy(void)
{
    uint32_t timeout = MPU6050_I2C_TIMEOUT;

    while ((DL_I2C_getControllerStatus(I2C_MPU6050_INST) &
            DL_I2C_CONTROLLER_STATUS_BUSY) != 0U) {
        if (timeout-- == 0U) {
            return false;
        }
    }

    return true;
}

static bool MPU6050_CheckError(void)
{
    return ((DL_I2C_getControllerStatus(I2C_MPU6050_INST) &
             DL_I2C_CONTROLLER_STATUS_ERROR) == 0U);
}
