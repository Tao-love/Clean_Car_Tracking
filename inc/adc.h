#ifndef ADC_H
#define ADC_H

#include <stdint.h>

#define LINE_SENSOR_COUNT      (8)
#define LINE_SENSOR_ACTIVE_LOW (0)

uint8_t LineSensor_ReadRaw(void);
int16_t LineSensor_GetError(void);

#endif /* ADC_H */
