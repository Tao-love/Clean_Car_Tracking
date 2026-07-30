/* MPU6050 仅提供 Z 轴角速度转向阻尼；不可用时所有接口保持零输出。 */
#ifndef MPU6050_H
#define MPU6050_H

#include <stdint.h>

void MPU6050_InitAndCalibrate(void);
/* 供未来启动流程显式调用；当前 PID 调参阶段不接入任何启动路径。 */
void MPU6050_RecalibrateForNextRun(void);
void MPU6050_Update(void);
int16_t MPU6050_GetTurnDamping(void);

#endif /* MPU6050_H */
