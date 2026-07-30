/* KEY1 启动的单一循线运行编排。 */
#ifndef LINE_RUN_H
#define LINE_RUN_H

#include <stdbool.h>

#define LINE_RUN_DURATION_TICKS (1500U)

bool LineRun_Init(void);
bool LineRun_Start(void);
/* 空闲时启动仅含编码器与左右速度 PI 的 10 秒双轮台架。 */
bool LineRun_StartBench(void);
/* 仅台架运行期间接受；下一控制 tick 同时用于左右轮。 */
bool LineRun_SetBenchTarget(int16_t target);
void LineRun_Stop(void);
void LineRun_ControlTick(void);
bool LineRun_IsRunning(void);
bool LineRun_IsBenchRunning(void);

/* UART 调参提交后清除循线与两侧速度 PI 的历史状态。 */
void LineRun_ResetForTuningUpdate(void);

#endif /* LINE_RUN_H */
