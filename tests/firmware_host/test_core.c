// ----- AI
/*
 * 协议基础模块的纯 C 测试契约。
 *
 * 该文件不访问 MSPM0 寄存器，用于检查 CRC 黄金向量和环形缓冲边界。
 * 本机尚无可执行 ARM 代码的模拟器，因此 CI 至少将它严格交叉编译；
 * 返回值检查在硬件单元测试固件中可直接复用。
 */

#include <stdbool.h>
#include <stdint.h>

#include "crc16.h"
#include "ring_buffer.h"

static int Test_CRC(void)
{
    static const uint8_t standardVector[] = "123456789";
    static const uint8_t frameBody[] = {
        0x01U, 0x01U, 0x01U, 0x34U, 0x12U, 0x03U, 0x00U, 0x61U, 0x62U, 0x63U
    };

    if (CRC16_Calculate(standardVector, 9U) != 0x29B1U) {
        return 1;
    }
    if (CRC16_Calculate(frameBody, sizeof(frameBody)) != 0x5807U) {
        return 2;
    }
    return 0;
}

static int Test_RingBuffer(void)
{
    uint8_t storage[4];
    uint8_t value = 0U;
    RingBuffer buffer;

    RingBuffer_Init(&buffer, storage, sizeof(storage));
    if (!RingBuffer_IsEmpty(&buffer) || RingBuffer_Count(&buffer) != 0U) {
        return 10;
    }
    if (!RingBuffer_PushFromISR(&buffer, 1U) ||
        !RingBuffer_PushFromISR(&buffer, 2U) ||
        !RingBuffer_PushFromISR(&buffer, 3U)) {
        return 11;
    }
    if (RingBuffer_PushFromISR(&buffer, 4U)) {
        return 12;
    }
    if (!RingBuffer_Pop(&buffer, &value) || value != 1U) {
        return 13;
    }
    if (!RingBuffer_PushFromISR(&buffer, 4U)) {
        return 14;
    }
    if (RingBuffer_Count(&buffer) != 3U) {
        return 15;
    }
    if (!RingBuffer_Pop(&buffer, &value) || value != 2U ||
        !RingBuffer_Pop(&buffer, &value) || value != 3U ||
        !RingBuffer_Pop(&buffer, &value) || value != 4U) {
        return 16;
    }
    if (RingBuffer_Pop(&buffer, &value) || !RingBuffer_IsEmpty(&buffer)) {
        return 17;
    }
    return 0;
}

int main(void)
{
    int result = Test_CRC();

    if (result != 0) {
        return result;
    }
    return Test_RingBuffer();
}
// ----- AI
