// ----- AI
/* 固定循线参数及其启动前范围检查。 */

#ifndef CONTROL_PARAMS_H
#define CONTROL_PARAMS_H

#include "control_types.h"

/* 参数全部有效时返回固定只读表，否则返回空指针并阻止启动。 */
const ControlParams *ControlParams_Get(void);

#endif /* CONTROL_PARAMS_H */
// ----- AI
