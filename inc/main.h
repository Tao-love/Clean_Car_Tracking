// ----- AI
/*
 * 应用调度器对外状态。
 * 当前只暴露 6.667 ms 单调 tick；ISR 只递增，主循环不清零它。
 */
#ifndef MAIN_H
#define MAIN_H

#include <stdint.h>

extern volatile uint32_t gControlTick;

#endif /* MAIN_H */
// ----- AI
