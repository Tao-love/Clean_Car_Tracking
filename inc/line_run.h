/* KEY1 启动的单一循线运行编排。 */
#ifndef LINE_RUN_H
#define LINE_RUN_H

#include <stdbool.h>

#define LINE_RUN_DURATION_TICKS (1500U)

bool LineRun_Init(void);
bool LineRun_Start(void);
void LineRun_Stop(void);
void LineRun_ControlTick(void);
bool LineRun_IsRunning(void);

/* UART 调参提交后清除循线与两侧速度 PI 的历史状态。 */
void LineRun_ResetForTuningUpdate(void);

#endif /* LINE_RUN_H */
