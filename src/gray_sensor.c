// ----- AI
/* 八路数字灰度 GPIO 采样与加权中心。 */

#include <stdbool.h>
#include <stdint.h>

#include "ti_msp_dl_config.h"
#include "gray_sensor.h"
#include "control_types.h"

uint8_t LineSensor_ReadRaw(void)
{
    uint8_t value = 0U;

    /* 读 D0 数字输入，高电平时置位 bit0。 */
    if (DL_GPIO_readPins(GPIO_LINE_LINE_D0_PORT, GPIO_LINE_LINE_D0_PIN) != 0U) {
        value |= (1U << 0);
    }
    /* 读 D1 数字输入，高电平时置位 bit1。 */
    if (DL_GPIO_readPins(GPIO_LINE_LINE_D1_PORT, GPIO_LINE_LINE_D1_PIN) != 0U) {
        value |= (1U << 1);
    }
    /* 读 D2 数字输入，高电平时置位 bit2。 */
    if (DL_GPIO_readPins(GPIO_LINE_LINE_D2_PORT, GPIO_LINE_LINE_D2_PIN) != 0U) {
        value |= (1U << 2);
    }
    /* 读 D3 数字输入，高电平时置位 bit3。 */
    if (DL_GPIO_readPins(GPIO_LINE_LINE_D3_PORT, GPIO_LINE_LINE_D3_PIN) != 0U) {
        value |= (1U << 3);
    }
    /* 读 D4 数字输入，高电平时置位 bit4。 */
    if (DL_GPIO_readPins(GPIO_LINE_LINE_D4_PORT, GPIO_LINE_LINE_D4_PIN) != 0U) {
        value |= (1U << 4);
    }
    /* 读 D5 数字输入，高电平时置位 bit5。 */
    if (DL_GPIO_readPins(GPIO_LINE_LINE_D5_PORT, GPIO_LINE_LINE_D5_PIN) != 0U) {
        value |= (1U << 5);
    }
    /* 读 D6/PA25 数字输入，该脚已被灰度占用，不是空闲电池 ADC。 */
    if (DL_GPIO_readPins(GPIO_LINE_LINE_D6_PORT, GPIO_LINE_LINE_D6_PIN) != 0U) {
        value |= (1U << 6);
    }
    /* 读 D7 数字输入，高电平时置位 bit7。 */
    if (DL_GPIO_readPins(GPIO_LINE_LINE_D7_PORT, GPIO_LINE_LINE_D7_PIN) != 0U) {
        value |= (1U << 7);
    }

#if LINE_SENSOR_ACTIVE_LOW
    value = (uint8_t) ~value;
#endif
    return value;
}

LineSensorSample LineSensor_ReadSample(void)
{
    static const int16_t weights[LINE_SENSOR_COUNT] = {
        -3500, -2500, -2200, -500, 500, 2200, 2500, 3500
    };
    LineSensorSample sample = {0U, 0U, false, false, 0};
    int32_t weightedSum = 0;
    uint8_t index;

    sample.bitmap = LineSensor_ReadRaw();
    for (index = 0U; index < LINE_SENSOR_COUNT; index++) {
        if ((sample.bitmap & (1U << index)) != 0U) {
            weightedSum += weights[index];
            sample.activeCount++;
        }
    }
    sample.valid = sample.activeCount != 0U;
    sample.specialPattern = sample.activeCount >= 6U;
    if (sample.valid) {
        sample.error = (int16_t) (weightedSum / sample.activeCount);
    }
    return sample;
}
// ----- AI
