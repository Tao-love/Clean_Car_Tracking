// ----- AI
/*
 * CRC16 模块。
 *
 * 职责：计算通信帧和 Flash 记录共用的 CRC-16/CCITT-FALSE。
 * 依赖：仅依赖标准整数类型。
 * 上下文：主循环可调用；函数无全局状态，ISR 也可调用，但不应在 ISR 计算整帧。
 * 硬件副作用：无。故障输出：无。
 */

#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>

/*
 * 用途：计算 CRC-16/CCITT-FALSE（poly=0x1021, init=0xFFFF）。
 * 输入：data 指向 length 字节；length 单位为字节，可为 0。
 * 输出：16 位 CRC；空数据的结果为 0xFFFF。
 * 前置条件：length>0 时 data 必须有效。
 * 副作用：无。ISR 可用性：可重入，但建议只在主循环使用。
 */
uint16_t CRC16_Calculate(const uint8_t *data, uint16_t length);

#endif /* CRC16_H */
// ----- AI
