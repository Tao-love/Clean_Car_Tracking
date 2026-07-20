#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>

void Encoder_Init(void);
void Encoder_UpdateSpeeds(void);
int32_t Encoder_GetLeftCount(void);
int32_t Encoder_GetRightCount(void);
int16_t Encoder_GetLeftSpeed(void);
int16_t Encoder_GetRightSpeed(void);
int16_t Encoder_GetLeftRawSpeed(void);
int16_t Encoder_GetRightRawSpeed(void);

#endif /* ENCODER_H */
