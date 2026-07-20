// ----- AI
/*
 * CRC-16/CCITT-FALSE 的逐字节实现。
 * 选择无查表版本是为了减少 Flash 占用；协议 payload 最大 128 字节，
 * 且 CRC 只在主循环计算，不会占用 100 Hz 控制 ISR。
 */

#include <stdint.h>

#include "crc16.h"

#define CRC16_INITIAL_VALUE (0xFFFFU)
#define CRC16_POLYNOMIAL    (0x1021U)

uint16_t CRC16_Calculate(const uint8_t *data, uint16_t length)
{
    uint16_t crc = CRC16_INITIAL_VALUE;
    uint16_t index;

    if ((data == 0) && (length != 0U)) {
        return 0U;
    }

    for (index = 0U; index < length; index++) {
        uint8_t bit;

        crc ^= (uint16_t) data[index] << 8;
        for (bit = 0U; bit < 8U; bit++) {
            if ((crc & 0x8000U) != 0U) {
                crc = (uint16_t) ((crc << 1) ^ CRC16_POLYNOMIAL);
            } else {
                crc = (uint16_t) (crc << 1);
            }
        }
    }
    return crc;
}
// ----- AI
