/* KEY4 编码器路程记录模块。 */
#ifndef RECORD_H
#define RECORD_H

#include <stdbool.h>
#include <stdint.h>

void Record_Init(void);
bool Record_Start(void);
bool Record_Stop(void);
bool Record_IsRecording(void);
int32_t Record_GetLeft(void);
int32_t Record_GetRight(void);

#endif /* RECORD_H */
