// ----- AI
/*
 * 最优参数 Flash 双槽模块。
 * 职责：在最后两个 1 KiB 主 Flash 扇区轮换保存 magic/schema/generation/length/params/CRC 记录。
 * 依赖：FlashCTL DriverLib、crc16、control_params 序列化。上下文：仅 IDLE→FLASH_WRITE 主循环路径。
 * 硬件副作用：擦除/编程主 Flash；操作期间短暂屏蔽中断，电机必须已统一停车。
 * 故障输出：明确的 FlashParamsResult；新槽读回或 CRC 错时旧槽仍保留。
 */

#ifndef FLASH_PARAMS_H
#define FLASH_PARAMS_H

#include <stdbool.h>
#include <stdint.h>

#include "autotune_types.h"

#define FLASH_PARAMS_SLOT_A_ADDRESS (0x0001F800UL)
#define FLASH_PARAMS_SLOT_B_ADDRESS (0x0001FC00UL)
#define FLASH_PARAMS_SLOT_SIZE      (1024U)
#define FLASH_PARAMS_RECORD_SIZE    (80U)

typedef enum {
    FLASH_PARAMS_OK = 0,
    FLASH_PARAMS_ALREADY_CURRENT = 1,
    FLASH_PARAMS_INVALID_ARGUMENT = 2,
    FLASH_PARAMS_ERASE_FAILED = 3,
    FLASH_PARAMS_PROGRAM_FAILED = 4,
    FLASH_PARAMS_VERIFY_FAILED = 5
} FlashParamsResult;

/* 用途：校验两槽并加载 generation 最大记录；两槽无效返回 false，不改输出。 */
bool FlashParams_LoadLatest(
    ControlParams *params, uint16_t *paramVersion, uint32_t *generation);

/*
 * 用途：把已明确选中的 active params 写到非当前槽，并读回校验。
 * 前置条件：上层状态已是 FLASH_WRITE，PWM=0，四方向脚低。禁止 ISR 调用。
 */
FlashParamsResult FlashParams_Commit(
    const ControlParams *params, uint16_t paramVersion);

#endif /* FLASH_PARAMS_H */
// ----- AI
