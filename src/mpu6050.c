/*
 * MPU6050 Gyro-Z turning damper.
 * The IMU is optional: any I2C or calibration failure disables it for this
 * power cycle and leaves the line controller with a zero damping command.
 */

#include <stdbool.h>
#include <stdint.h>

#include "ti_msp_dl_config.h"
#include "autotune_types.h"
#include "mpu6050.h"

#define MPU6050_I2C_ADDR                 (0x68U)
#define MPU6050_REG_SMPLRT_DIV           (0x19U)
#define MPU6050_REG_CONFIG               (0x1AU)
#define MPU6050_REG_GYRO_CONFIG          (0x1BU)
#define MPU6050_REG_GYRO_ZOUT_H          (0x47U)
#define MPU6050_REG_PWR_MGMT_1           (0x6BU)
#define MPU6050_REG_WHO_AM_I             (0x75U)
#define MPU6050_WHO_AM_I_VALUE           (0x68U)
#define MPU6050_CALIBRATION_SAMPLES      (200U)
#define MPU6050_I2C_POLL_LIMIT           (10000U)
#define MPU6050_GYRO_SIGN                (1)
#define MPU6050_GYRO_KD_Q16              (Q16_ONE / 20)
#define MPU6050_GYRO_Z_LSB_PER_DPS       (164)

static bool gImuEnabled;
static int16_t gGyroZOffset;
static int32_t gGyroZCdps;

static void MPU6050_Disable(void);
static bool MPU6050_WriteReg(uint8_t reg, uint8_t value);
static bool MPU6050_ReadRegs(uint8_t reg, uint8_t *data, uint8_t length);
static bool MPU6050_ReadGyroZRaw(int16_t *gyroZRaw);
static bool MPU6050_WaitIdle(void);
static bool MPU6050_WaitNotBusy(void);
static bool MPU6050_WaitRxByte(void);
static bool MPU6050_HasControllerError(void);
static int16_t MPU6050_ClampI16(int32_t value);

void MPU6050_InitAndCalibrate(void)
{
    uint8_t whoAmI = 0U;
    uint16_t index;
    int32_t sum = 0;
    int16_t gyroZRaw = 0;

    MPU6050_Disable();
    delay_cycles(CPUCLK_FREQ / 10U);

    if (!MPU6050_ReadRegs(MPU6050_REG_WHO_AM_I, &whoAmI, 1U) ||
        (whoAmI != MPU6050_WHO_AM_I_VALUE) ||
        !MPU6050_WriteReg(MPU6050_REG_PWR_MGMT_1, 0x01U)) {
        return;
    }
    delay_cycles(CPUCLK_FREQ / 100U);
    if (!MPU6050_WriteReg(MPU6050_REG_SMPLRT_DIV, 0x07U) ||
        !MPU6050_WriteReg(MPU6050_REG_CONFIG, 0x03U) ||
        !MPU6050_WriteReg(MPU6050_REG_GYRO_CONFIG, 0x18U)) {
        return;
    }

    gImuEnabled = true;
    for (index = 0U; index < MPU6050_CALIBRATION_SAMPLES; index++) {
        if (!MPU6050_ReadGyroZRaw(&gyroZRaw)) {
            MPU6050_Disable();
            return;
        }
        sum += gyroZRaw;
        delay_cycles(CPUCLK_FREQ / 1000U);
    }
    gGyroZOffset = (int16_t) (sum / (int32_t) MPU6050_CALIBRATION_SAMPLES);
}

void MPU6050_Update(void)
{
    int16_t gyroZRaw;

    if (!gImuEnabled) {
        return;
    }
    if (!MPU6050_ReadGyroZRaw(&gyroZRaw)) {
        MPU6050_Disable();
        return;
    }
    /* ±2000 dps is 16.4 LSB/(dps), so cdps = raw * 1000 / 164. */
    gGyroZCdps = (((int32_t) gyroZRaw - gGyroZOffset) * 1000) /
        MPU6050_GYRO_Z_LSB_PER_DPS;
}

int16_t MPU6050_GetTurnDamping(void)
{
    int64_t damping;

    if (!gImuEnabled) {
        return 0;
    }
    /* KD is target-speed per deg/s; gGyroZCdps is centi-deg/s. */
    damping = (int64_t) MPU6050_GYRO_SIGN * MPU6050_GYRO_KD_Q16 *
        gGyroZCdps;
    damping /= ((int64_t) Q16_ONE * 100);
    return MPU6050_ClampI16((int32_t) damping);
}

static void MPU6050_Disable(void)
{
    gImuEnabled = false;
    gGyroZOffset = 0;
    gGyroZCdps = 0;
}

static bool MPU6050_WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};

    if (!MPU6050_WaitIdle() ||
        (DL_I2C_fillControllerTXFIFO(I2C_MPU6050_INST, data,
            sizeof(data)) != sizeof(data))) {
        return false;
    }
    DL_I2C_startControllerTransfer(I2C_MPU6050_INST, MPU6050_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_TX, sizeof(data));
    delay_cycles(100U);
    return MPU6050_WaitNotBusy() && !MPU6050_HasControllerError();
}

static bool MPU6050_ReadRegs(uint8_t reg, uint8_t *data, uint8_t length)
{
    uint8_t index;

    if ((data == 0) || (length == 0U) || !MPU6050_WaitIdle() ||
        (DL_I2C_fillControllerTXFIFO(I2C_MPU6050_INST, &reg, 1U) != 1U)) {
        return false;
    }
    DL_I2C_startControllerTransfer(I2C_MPU6050_INST, MPU6050_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_TX, 1U);
    delay_cycles(100U);
    if (!MPU6050_WaitNotBusy() || MPU6050_HasControllerError() ||
        !MPU6050_WaitIdle()) {
        return false;
    }

    DL_I2C_startControllerTransfer(I2C_MPU6050_INST, MPU6050_I2C_ADDR,
        DL_I2C_CONTROLLER_DIRECTION_RX, length);
    for (index = 0U; index < length; index++) {
        if (!MPU6050_WaitRxByte()) {
            return false;
        }
        data[index] = DL_I2C_receiveControllerData(I2C_MPU6050_INST);
    }
    return MPU6050_WaitNotBusy() && !MPU6050_HasControllerError();
}

static bool MPU6050_ReadGyroZRaw(int16_t *gyroZRaw)
{
    uint8_t data[2];

    if ((gyroZRaw == 0) || !MPU6050_ReadRegs(
            MPU6050_REG_GYRO_ZOUT_H, data, sizeof(data))) {
        return false;
    }
    *gyroZRaw = (int16_t) (((uint16_t) data[0] << 8) | data[1]);
    return true;
}

static bool MPU6050_WaitIdle(void)
{
    uint32_t count = MPU6050_I2C_POLL_LIMIT;

    while ((DL_I2C_getControllerStatus(I2C_MPU6050_INST) &
            DL_I2C_CONTROLLER_STATUS_IDLE) == 0U) {
        if (MPU6050_HasControllerError() || (--count == 0U)) {
            return false;
        }
    }
    return true;
}

static bool MPU6050_WaitNotBusy(void)
{
    uint32_t count = MPU6050_I2C_POLL_LIMIT;

    while ((DL_I2C_getControllerStatus(I2C_MPU6050_INST) &
            DL_I2C_CONTROLLER_STATUS_BUSY) != 0U) {
        if (MPU6050_HasControllerError() || (--count == 0U)) {
            return false;
        }
    }
    return true;
}

static bool MPU6050_WaitRxByte(void)
{
    uint32_t count = MPU6050_I2C_POLL_LIMIT;

    while (DL_I2C_isControllerRXFIFOEmpty(I2C_MPU6050_INST)) {
        if (MPU6050_HasControllerError() || (--count == 0U)) {
            return false;
        }
    }
    return true;
}

static bool MPU6050_HasControllerError(void)
{
    return (DL_I2C_getControllerStatus(I2C_MPU6050_INST) &
        DL_I2C_CONTROLLER_STATUS_ERROR) != 0U;
}

static int16_t MPU6050_ClampI16(int32_t value)
{
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t) value;
}
