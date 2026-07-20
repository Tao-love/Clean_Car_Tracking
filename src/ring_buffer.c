// ----- AI
/*
 * 环形缓冲实现。
 * 始终留一个空槽区分“空”和“满”，因此可用容量为 storageSize-1。
 * MSPM0 Cortex-M0+ 对齐的 16 位读写为单条指令；单生产者/单消费者不需要关中断。
 */

#include <stdbool.h>
#include <stdint.h>

#include "ring_buffer.h"

void RingBuffer_Init(RingBuffer *buffer, uint8_t *storage, uint16_t storageSize)
{
    if (buffer == 0) {
        return;
    }
    buffer->storage = storage;
    buffer->capacity = ((storage != 0) && (storageSize >= 2U)) ? storageSize : 0U;
    buffer->head = 0U;
    buffer->tail = 0U;
}

bool RingBuffer_PushFromISR(RingBuffer *buffer, uint8_t value)
{
    uint16_t next;

    if ((buffer == 0) || (buffer->capacity < 2U)) {
        return false;
    }
    next = (uint16_t) (buffer->head + 1U);
    if (next >= buffer->capacity) {
        next = 0U;
    }
    if (next == buffer->tail) {
        return false;
    }
    buffer->storage[buffer->head] = value;
    buffer->head = next;
    return true;
}

bool RingBuffer_Pop(RingBuffer *buffer, uint8_t *value)
{
    uint16_t next;

    if ((buffer == 0) || (value == 0) || (buffer->capacity < 2U) ||
        (buffer->head == buffer->tail)) {
        return false;
    }
    *value = buffer->storage[buffer->tail];
    next = (uint16_t) (buffer->tail + 1U);
    if (next >= buffer->capacity) {
        next = 0U;
    }
    buffer->tail = next;
    return true;
}

uint16_t RingBuffer_Count(const RingBuffer *buffer)
{
    uint16_t head;
    uint16_t tail;

    if ((buffer == 0) || (buffer->capacity < 2U)) {
        return 0U;
    }
    head = buffer->head;
    tail = buffer->tail;
    if (head >= tail) {
        return (uint16_t) (head - tail);
    }
    return (uint16_t) (buffer->capacity - tail + head);
}

bool RingBuffer_IsEmpty(const RingBuffer *buffer)
{
    return RingBuffer_Count(buffer) == 0U;
}
// ----- AI
