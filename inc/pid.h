#ifndef PID_H
#define PID_H

#include <stdint.h>

void PID_Init(void);
int16_t PID_Calculate(int16_t error);

#endif /* PID_H */
