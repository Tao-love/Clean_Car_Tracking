// ----- AI
/* UART1 中断搬运实现：高优先级 ACK/汇总始终先于可丢遥测发送。 */

#include <stdbool.h>
#include <stdint.h>

#include "ti_msp_dl_config.h"
#include "ring_buffer.h"
#include "uart_transport.h"

#define UART_RX_STORAGE_SIZE      (256U)
#define UART_TX_HIGH_STORAGE_SIZE (512U)
#define UART_TX_LOW_STORAGE_SIZE  (256U)

static uint8_t gRxStorage[UART_RX_STORAGE_SIZE];
static uint8_t gTxHighStorage[UART_TX_HIGH_STORAGE_SIZE];
static uint8_t gTxLowStorage[UART_TX_LOW_STORAGE_SIZE];
static RingBuffer gRxBuffer;
static RingBuffer gTxHighBuffer;
static RingBuffer gTxLowBuffer;
static volatile UARTTransportCounters gCounters;

static bool UART_Transport_PopNextTx(uint8_t *value);

void UART_Transport_Init(void)
{
    RingBuffer_Init(&gRxBuffer, gRxStorage, sizeof(gRxStorage));
    RingBuffer_Init(&gTxHighBuffer, gTxHighStorage, sizeof(gTxHighStorage));
    RingBuffer_Init(&gTxLowBuffer, gTxLowStorage, sizeof(gTxLowStorage));
    gCounters.rxOverflowBytes = 0U;
    gCounters.txHighRejectedFrames = 0U;
    gCounters.txLowRejectedFrames = 0U;

    /* 清除上电过程可能残留的 UART1 NVIC 请求，避免初始化后立即进入旧中断。 */
    NVIC_ClearPendingIRQ(UART_DEBUG_INST_INT_IRQN);
    /* 允许 UART1 中断进入 CPU；SysConfig 已只预开 RX 来源，TX 在有数据时动态开启。 */
    NVIC_EnableIRQ(UART_DEBUG_INST_INT_IRQN);
}

bool UART_Transport_ReadByte(uint8_t *value)
{
    return RingBuffer_Pop(&gRxBuffer, value);
}

bool UART_Transport_Write(
    const uint8_t *data, uint16_t length, bool highPriority)
{
    RingBuffer *target = highPriority ? &gTxHighBuffer : &gTxLowBuffer;
    uint16_t index;

    if (((data == 0) && (length != 0U)) ||
        (RingBuffer_Free(target) < length)) {
        if (highPriority) {
            gCounters.txHighRejectedFrames++;
        } else {
            gCounters.txLowRejectedFrames++;
        }
        return false;
    }

    for (index = 0U; index < length; index++) {
        (void) RingBuffer_PushFromISR(target, data[index]);
    }

    /* TX 队列已有数据，开启 UART1 TX FIFO 中断让 ISR 开始发送，主循环不等待硬件。 */
    DL_UART_Main_enableInterrupt(UART_DEBUG_INST, DL_UART_MAIN_INTERRUPT_TX);
    return true;
}

UARTTransportCounters UART_Transport_GetCounters(void)
{
    UARTTransportCounters result;

    /* 短暂屏蔽 UART1 IRQ，保证三个 32 位计数属于同一个快照。 */
    NVIC_DisableIRQ(UART_DEBUG_INST_INT_IRQN);
    result = gCounters;
    /* 快照完成后立即恢复 UART1 IRQ，已到达的字节仍保留在硬件 FIFO。 */
    NVIC_EnableIRQ(UART_DEBUG_INST_INT_IRQN);
    return result;
}

void UART_DEBUG_INST_IRQHandler(void)
{
    switch (DL_UART_Main_getPendingInterrupt(UART_DEBUG_INST)) {
        case DL_UART_MAIN_IIDX_RX:
            /* 一次清空硬件 RX FIFO，降低 9600 baud 连续字节的溢出风险。 */
            while (!DL_UART_Main_isRXFIFOEmpty(UART_DEBUG_INST)) {
                uint8_t value;

                /* 读 RXDATA 会从 UART1 硬件 FIFO 弹出一字节。 */
                value = DL_UART_Main_receiveData(UART_DEBUG_INST);
                if (!RingBuffer_PushFromISR(&gRxBuffer, value)) {
                    gCounters.rxOverflowBytes++;
                }
            }
            break;
        case DL_UART_MAIN_IIDX_TX:
            /* 当硬件 TX FIFO 还有空位时，尽量先填充高优先级数据。 */
            while (!DL_UART_Main_isTXFIFOFull(UART_DEBUG_INST)) {
                uint8_t value;

                if (!UART_Transport_PopNextTx(&value)) {
                    /* 软件 TX 队列已空，关闭 TX 中断避免空转；RX 中断保持开启。 */
                    DL_UART_Main_disableInterrupt(
                        UART_DEBUG_INST, DL_UART_MAIN_INTERRUPT_TX);
                    break;
                }
                /* 写 TXDATA 把一字节加入 UART1 硬件发送 FIFO。 */
                DL_UART_Main_transmitData(UART_DEBUG_INST, value);
            }
            break;
        default:
            break;
    }
}

static bool UART_Transport_PopNextTx(uint8_t *value)
{
    if (RingBuffer_Pop(&gTxHighBuffer, value)) {
        return true;
    }
    return RingBuffer_Pop(&gTxLowBuffer, value);
}
// ----- AI
