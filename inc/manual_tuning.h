/*
 * 手动烧录调参表。
 * 职责：提供每次编译烧录后唯一的启动参数来源。
 * 依赖：autotune_types。硬件副作用：无。
 */

#ifndef MANUAL_TUNING_H
#define MANUAL_TUNING_H

#include "autotune_types.h"

/* 返回人工编辑的完整参数表；调用方不得修改返回内容。 */
const ControlParams *ManualTuning_GetParams(void);

#endif /* MANUAL_TUNING_H */
