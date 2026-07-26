/* 单一循线控制链共享的数据类型。 */
#ifndef CONTROL_TYPES_H
#define CONTROL_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#define Q16_ONE (65536L)

typedef struct {
    int32_t speedKpLeftQ16;
    int32_t speedKiLeftQ16;
    int32_t speedFeedforwardLeftQ16;
    int32_t speedKpRightQ16;
    int32_t speedKiRightQ16;
    int32_t speedFeedforwardRightQ16;
    int32_t lineKpQ16;
    int32_t lineKdQ16;
    int32_t derivativeAlphaQ16;
    int32_t speedIntegralLimit;
    int16_t baseSpeed;
    int16_t maxTargetSpeed;
    int16_t maxDeltaSpeed;
    int16_t maxPwm;
    int16_t derivativeLimit;
} ControlParams;

typedef struct {
    uint8_t bitmap;
    uint8_t activeCount;
    bool valid;
    bool specialPattern;
    int16_t error;
} LineSensorSample;

#endif /* CONTROL_TYPES_H */
