# USB_nobluetooth：MSPM0G3507 手动烧录调参固件

这是不依赖蓝牙或串口调参的 MSPM0G3507 小车固件。每次改变参数后，重新编译、烧录，再用 KEY1 完成一次最多 5 秒的本地循线试验。UART2 仍只是原样回显诊断通道，不参与启动或调参。

## 已实现的运行路径

```text
编辑 src/manual_tuning.c
→ 编译、烧录、上电
→ 放车在线上，短按 KEY1
→ 100 Hz 循线 + 左右速度 PI（最多 5 秒）
→ 正常结束：可再次按 KEY1
→ 安全故障：停车并锁存，必须复位/重新上电后再试
```

- `KEY1` 是 `PA23` 上拉输入，按下沿只触发一次 `TRIAL_MODE_LINE_FOLLOW`。左右目标速度由 `baseSpeed` 与循线 PD 计算，KEY1 不再启动固定 `6/6` 的轮速试验。
- 本次上电的参数只来自 [`src/manual_tuning.c`](src/manual_tuning.c)。旧 Flash 参数不会被读取或覆盖源码表。
- 正常运行满 500 个 10 ms tick（5 秒）后立即回到 `IDLE`，可再次按 KEY1。丢线、堵转、控制超期等故障保持 `FAULT`，KEY1 不会清除故障；先排查后复位。
- 100 Hz 控制、八路灰度、编码器速度 PI、5 秒上限、丢线/堵转/控制超期保护均保留。
- `maxPwm` 的固件上限是 `400`，PWM 满量程是 `1600`。当前手动表为 `200`；在完成低速验证前不要增大它。

## 唯一需要编辑的参数表

只编辑 [`src/manual_tuning.c`](src/manual_tuning.c)，不要改 `syscfg_gen/`、`Debug/` 或 Flash 槽地址。增益是 Q16.16：`1.0 = Q16_ONE = 65536`；`baseSpeed`、`maxTargetSpeed` 和 `maxDeltaSpeed` 的单位均为“每 10 ms 编码器计数”，不是 PWM 百分比。

当前保守基线：

- 左/右速度：`Kp=8/8`、`Ki=0/0`、前馈=`28/24`。
- 循线：`Kp=0`、`Kd=0`，因此它是安全的“无转向修正”基线，**不能据此期待完成循线**；首次落地前由调参记录给出小的非零 `Kp`，`Kd` 先保持 0。
- `baseSpeed=5`、`maxPwm=200`、堵转阈值=`PWM 80` 且速度 `<1`。

## 每轮手动调参流程

1. 在 `src/manual_tuning.c` 只改本轮有依据的一组参数，记录实际值；编译并烧录。烧录前确认电池状态、接线、轮胎、灰度高度和赛道没有变化。
2. 首次或输出增大后先悬空确认两轮均能转且正向 PWM 的编码器速度为正；发现反向、卡轮或无力，先处理电机/编码器/机械问题，不增大 PID。
3. 落地时把车放在黑线上，朝向赛道前进方向；短按 KEY1，只观察一轮，最长 5 秒。方向反了或明显失控立刻断开电机电源。
4. 先固定低 `baseSpeed`，调循线 `Kp` 到能回到线附近；再少量加入 `Kd` 抑制摆动；最后逐档提高 `baseSpeed`。速度 PI、前馈或 PWM 饱和异常要先回到低输出排查。
5. 正常结束可再次按 KEY1。若约 0.15 秒停车，优先检查是否丢线；若约 0.3 秒停车，优先检查堵转、轮子和电源；故障后先复位再进行下一轮。

每次反馈请包含：模式（KEY1 手动循线/车轮悬空）、落地与起点、实际烧录的 `baseSpeed`、左右前馈、左右 Kp/Ki、循线 Kp/Kd、`maxPwm`，以及运行时长、左右轮现象、循线现象和停止前表现。

## 编译验证与烧录

离线验证（不烧录、不打开串口、不驱动电机）：

```powershell
cd E:\TI_work\TI_Project\USB_nobluetooth
powershell -ExecutionPolicy Bypass -File .\tests\verify_manual_tuning.ps1
powershell -ExecutionPolicy Bypass -File .\tools\verify.ps1
```

使用 CCS 导入本目录工程；可编辑的 SysConfig 源为 `empty.syscfg`，生成文件仅供读取。`targetConfigs/MSPM0G3507.ccxml` 当前配置为 XDS110；若使用 CH340 UART BSL 下载，请使用经过确认的 UART BSL 连接配置和厂商提供的 BOOT/RESET 时序。UART BSL 不提供在线调试。

## UART2 诊断（非调参通道）

PCB `USART2` 为 `9600 8N1`：`TX2/PB15 → USB-TTL RXD`、`RX2/PB16 → USB-TTL TXD`、GND 对 GND；不要向板子接 USB-TTL 电源。复位后会发送一次 `UART2 READY\r\n`，收到字节会原样回显。该通道不会接受参数、不会启动电机，也不会解除故障。
