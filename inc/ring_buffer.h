// ----- AI
/*
 * 单生产者、单消费者字节环形缓冲。
 *
 * 职责：在 UART ISR 和主循环之间传递字节，不解析帧。
 * 依赖：标准整数与布尔类型。
 * 上下文：RX 使用 ISR push/主循环 pop；TX 使用主循环 push/ISR pop。
 * 数据单位：字节。硬件副作用：无。
 * 故障输出：push 返回 false 表示队列已满，由调用者计数或丢弃低优先级数据。
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t *storage;
    uint16_t capacity;
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuffer;

/*
 * 用途：绑定由调用者持有的存储区并清空队列。
 * 输入：buffer、storage 和 storageSize（字节）。
 * 输出：无。前置条件：指针有效，storageSize>=2。
 * 副作用：重置索引，不清零 storage 内容。ISR 可用性：不允许。
 */
void RingBuffer_Init(RingBuffer *buffer, uint8_t *storage, uint16_t storageSize);

/*
 * 用途：由单一生产者写入一字节。
 * 输出：true=成功，false=队列已满或未初始化。
 * 副作用：成功时推进 head。ISR 可用性：允许。
 */
bool RingBuffer_PushFromISR(RingBuffer *buffer, uint8_t value);

/*
 * 用途：由单一消费者取出一字节。
 * 输出：true=成功并写入 value，false=队列空或参数无效。
 * 副作用：成功时推进 tail。ISR 可用性：允许。
 */
bool RingBuffer_Pop(RingBuffer *buffer, uint8_t *value);

/* 用途：返回当前可读字节数；只读，ISR 可用。 */
uint16_t RingBuffer_Count(const RingBuffer *buffer);

/* 用途：检查队列是否为空；只读，ISR 可用。 */
bool RingBuffer_IsEmpty(const RingBuffer *buffer);

#endif /* RING_BUFFER_H */
// ----- AI
