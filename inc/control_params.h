// ----- AI
/* 固定循线参数及其启动前范围检查。 */

#ifndef CONTROL_PARAMS_H
#define CONTROL_PARAMS_H

#include "control_types.h"

/* 参数全部有效时返回活动只读表，否则返回空指针并阻止启动。 */
const ControlParams *ControlParams_Get(void);

/* 仅供 UART 自动调参模块提交已校验的运行时参数。 */
ControlParams *ControlParams_GetMutableForUart(void);

#endif /* CONTROL_PARAMS_H */
// ----- AI
