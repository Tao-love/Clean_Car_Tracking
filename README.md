# Clean_Car：MSPM0G3507 单一循线固件

本工程只运行 KEY1 启动的自动循线：八路数字灰度位置误差经过循线 PD，形成左右目标速度；左右速度 PI 与前馈根据编码器速度计算 PWM；MPU6050 Z 轴角速度提供转弯阻尼。控制周期为 6.667 ms（150 Hz），每轮最多 1500 个控制 tick，约 10 秒。

## 当前运行方式

上电初始化完成后电机保持停车：

1. IDLE 时按下并松开 KEY1，启动一轮全新的循线。
2. RUNNING 时再次按下 KEY1，立即统一停车并返回 IDLE。
3. 停车会清除 1500 tick 计时、左右速度 PI、循线 PD 和本轮最后灰度误差；必须再次按 KEY1 才会重新启动。
4. 正常运行到第 1500 个 tick 后立即停车并回到 IDLE，可再次按 KEY1。

灰度有效时使用当前误差；灰度无效时持续使用本轮最后一次有效误差。本轮尚未出现有效样本时使用误差 0，因此车辆仍按基础速度前进，直到重新识线、KEY1 停车或达到 10 秒上限。

当前没有丢线、堵转、控制超期或故障锁存状态机。仍保留统一 `Motor_Stop()`、目标速度限幅、差速限幅、速度 PI 积分/PWM 限幅、`maxPwm <= 400` 固件范围检查和 10 秒运行上限。丢线延续是有意行为，不等同于安全停车。

核心文件：

- `src/line_run.c`：五个运行 API、KEY1 运行状态、1500 tick 上限和灰度误差延续。
- `src/control_params.c`：唯一固定实车参数表及启动前范围检查。
- `src/line_control.c`：循线 PD、差速斜率限制和单轮目标限幅。
- `src/speed_pi.c`：左右速度 PI、前馈、积分限幅和 PWM 限幅。
- `src/mpu6050.c`：MPU6050 初始化、校准与转弯阻尼；设备不可用时阻尼为 0。
- `empty.syscfg`：全部引脚、时钟和外设配置的唯一可编辑源。

## 固定控制参数

增益使用 Q16.16；速度单位是“每 6.667 ms 目标编码器计数”，不是 PWM 百分比。PWM 满量程为 1600。

| 参数 | 当前值 |
| --- | ---: |
| 左速度 Kp / Ki / 前馈 | 12 / 0 / 7 |
| 右速度 Kp / Ki / 前馈 | 12 / 0 / 6 |
| 循线 Kp / Kd | 1/70 / 1/320 |
| D 项滤波系数 | 5/8 |
| 速度积分限幅 | 10000 |
| baseSpeed | 27 |
| maxTargetSpeed / maxDeltaSpeed | 67 / 67 |
| maxPwm | 300（固件硬上限 400） |
| derivativeLimit | 2333 |

修改参数只编辑 `src/control_params.c`，随后必须重新执行严格验证、编译和烧录。不要手工修改 `syscfg_gen/`。

## 保留的基础硬件配置

OLED、蜂鸣器、超声波、LED、KEY2～KEY4、KEY1 和 MPU6050 的 SysConfig 基础配置均保留。UART2 也只保留 PB15/PB16、9600 8N1 基础配置；固件不启用其 RX 中断、不建立队列，也不收发运行数据。

唯一烧录与调试入口是 `targetConfigs/MSPM0G3507.ccxml` 中的 TI XDS110。`.ccsproject` 内的 `projectspec.basic-1` 只是 TI 官方来源模板标识，不是工程名或第二个烧录入口。

## 离线验证与构建

在 PowerShell 中运行：

```powershell
cd E:\TI_work\TI_Project\Clean_Car
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\verify.ps1
Get-ChildItem .\tests\verify_*.ps1 | ForEach-Object {
    powershell -NoProfile -ExecutionPolicy Bypass -File $_.FullName
}
git diff --check
```

`tools/verify.ps1` 使用正式 SysConfig CLI 从 `empty.syscfg` 重新生成 `syscfg_gen/*`，再用 TI Arm Clang 的 `-Wall -Wextra -Werror` 编译链接全部固件及契约测试。成功输出为：

```text
.build\verify\Clean_Car.out
```

SysConfig 对 PWM 在 STOP/STANDBY 模式下不保持寄存器的提示属于 TI 外设信息；本固件没有进入这些低功耗模式。验证脚本不烧录、不连接串口，也不启动电机。

## 实车验证顺序

1. 首次或参数变大后先让车轮悬空，使用 XDS110 烧录 `Clean_Car.out`；确认上电不转、KEY1 能启动、运行中再按一次能立即停车。
2. 悬空等待一轮，确认约 10 秒自动停车；方向相反、卡轮、异响或明显失控时立即断开电机电源。
3. 再把车放到黑线上低风险落地验证循线。主动让灰度短暂无效，确认车辆延续最后一次有效误差，并准备随时按 KEY1 或断开电机电源。
4. 源码/编译验证不能代替以上实车步骤；只有实际观察后才能确认机械方向、传感器逻辑和循线效果。

## Git 留存与恢复

- `clean-car-working-line-baseline`：简化前“实车巡线成功”的完整快照。
- `clean-car-line-only-v1`：本次单一循线版本完成并验证后的标签。
- 当前实施分支：`clean-car-simplification`。

恢复单个旧文件：

```powershell
git restore --source clean-car-working-line-baseline -- path\to\file
```

完整回到简化前并保留当前分支：

```powershell
git switch -c restore-working-line clean-car-working-line-baseline
```

恢复某一整组功能时，先用 `git log --oneline clean-car-working-line-baseline..clean-car-line-only-v1` 找到对应删除提交，再在新恢复分支上执行 `git revert <提交>`。当前未推送远端；网络恢复后再决定远端仓库位置。
