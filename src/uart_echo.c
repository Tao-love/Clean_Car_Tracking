// ----- AI
/* UART 原样回显轮询实现。 */

#include <stdint.h>

#include "uart_echo.h"

uint16_t UART_Echo_Poll(UART_EchoReadByte readByte,
    UART_EchoWriteBytes writeBytes, uint16_t byteBudget)
{
    uint16_t consumed = 0U;
    uint8_t value;

    if ((readByte == 0) || (writeBytes == 0)) {
        return 0U;
    }
    while ((consumed < byteBudget) && readByte(&value)) {
        (void) writeBytes(&value, 1U, true);
        consumed++;
    }
    return consumed;
}
// ----- AI
