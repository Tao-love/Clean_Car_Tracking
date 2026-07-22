# 1-3 无线自动整定实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在队友的 MSPM0G3507 循迹工程副本上，完整实现已批准 `1-3.md` 的本地 100 Hz 双闭环、5 秒试验、本地安全、JDY-31 可靠通信、Python 自动整定、Flash 保存和 VOFA+ 转发。

**Architecture:** MCU 上的字节搬运、帧协议、状态机、安全、控制、统计和 Flash 模块各自独立，主循环按单调 10 ms tick 协作调度。Python 独占蓝牙 COM 口，使用同一二进制协议编排试验，并把观察数据可选转发给 VOFA+。

**Tech Stack:** MSPM0 SDK 2.11.00.07 DriverLib、TI Arm Clang、SysConfig、C11、Python 3.13、pyserial、numpy、unittest。

**执行状态（2026-07-20）：** 本计划的软件、文档、离线测试、SysConfig 重生成和固件编译链接已经完成。用户明确本次验收不要求烧录、JDY-31 实连、电机运行或赛道实测；对应 hardware gate 以“本次验收豁免”闭环，并保留在 `docs/hardware-test-checklist.md` 供以后执行。`[x]` 表示该步骤的软件交付已完成，或该步骤是已明确标注的本次实物验收豁免，不表示虚构实测结果。

## Global Constraints

- 实施唯一依据是已批准 `1-3`；仓库内摘要为 `docs/specs/1-3-summary.md`。
- 不修改原始目录 `friend_fst_par`；修改副本最终交付为 `E:\TI_work\TI_Project\basic-1`。
- TB6612 `STBY` 固定 5 V；统一停车必须同时清零两路 PWM 和四路方向脚。
- 控制周期固定 10 ms，每次 trial 最多 500 tick，心跳 100 ms，通信超时 400 ms，堵转 300 ms，丢线 150 ms。
- UART1 使用 PA8/TX、PA9/RX，首次联调为 9600 baud，遥测默认 10 Hz。
- 增益一律使用有符号 Q16.16；RUNNING 期间禁止替换参数。
- 电池 ADC、电流检测、MPU6050 闭环和直角专用状态机不进入首版。PA25 已被 `LINE_D6` 占用，不得按旧文档当作空闲 ADC 脚。
- 人工 C 注释用清晰中文，新增块前后用 `// ----- AI`；Python 用 `# ----- AI`；SysConfig 生成文件不手工加标记。
- 不对未经实车标定的启动 PWM、堵转速度和轮径伪造数值；它们必须是受限配置并使用保守默认。

---

## 文件边界

### MCU 固件

- `inc/autotune_types.h`：全系统状态、故障、停止原因、参数和统计的稳定数据类型。
- `inc/ring_buffer.h`, `src/ring_buffer.c`：ISR/主循环单生产者单消费者字节环形队列。
- `inc/crc16.h`, `src/crc16.c`：CRC-16/CCITT-FALSE。
- `inc/uart_transport.h`, `src/uart_transport.c`：UART1 RX/TX ISR 和非阻塞队列，不解析协议。
- `inc/protocol.h`, `src/protocol.c`：流式重同步、帧编解码、Sequence/Session 去重和 ACK/NACK。
- `inc/control_params.h`, `src/control_params.c`：参数范围校验、pending/active 原子切换和 `paramVersion`。
- `inc/speed_pi.h`, `src/speed_pi.c`：左右轮 Q16.16 PI、前馈、条件积分和限幅。
- `inc/line_control.h`, `src/line_control.c`：位图加权误差、差分低通、PD 和分层限幅。
- `inc/safety_guard.h`, `src/safety_guard.c`：统一停车、心跳/丢线/堵转/超期联锁。
- `inc/trial_stats.h`, `src/trial_stats.c`：500 样本的 64 位饱和统计和汇总。
- `inc/trial_manager.h`, `src/trial_manager.c`：`BOOT_SAFE/IDLE/ARMED/RUNNING/FINISHED/FAULT/FLASH_WRITE` 状态机。
- `inc/flash_params.h`, `src/flash_params.c`：双槽 generation+CRC 加载与显式提交。
- `inc/app_protocol.h`, `src/app_protocol.c`：命令 payload 和业务模块之间的唯一粘合层。
- `src/main.c`：只负责初始化、单调 tick 调度和各模块 poll。
- `empty.syscfg`：UART1 9600 baud 及 RX/TX 中断；由 SysConfig 重生成 `syscfg_gen`。

### Python 上位机

- `python/autotune/protocol.py`：与 MCU 逐字节一致的帧、CRC、枚举和 payload。
- `python/autotune/link.py`：串口读写线程、ACK 等待、重试和原始帧日志。
- `python/autotune/session.py`：HELLO、心跳、状态镜像和故障流程。
- `python/autotune/trial.py`：SET_PARAMS→ARM→START→SUMMARY、失败暂停和安全回滚。
- `python/autotune/scoring.py`：可复算的归一化分数和不可被速度抵消的失败惩罚。
- `python/autotune/optimizer.py`：有边界网格/坐标搜索和 last-safe 回滚点。
- `python/autotune/vofa.py`：可选本机 TCP/UDP 转发，任何异常都不影响试验。
- `python/autotune/cli.py`：配置加载、COM 口列表、链路测试、单次试验、分阶段整定和参数提交入口。
- `python/tests/`：协议、会话、评分、优化器和 VOFA 故障隔离的单元测试。

## Task 1: 固化基线与可构建配置

**Files:**
- Create: `.gitignore`
- Modify: `.cproject`, `.ccsproject`, `empty.syscfg`, `README.md`
- Generate: `syscfg_gen/ti_msp_dl_config.c`, `syscfg_gen/ti_msp_dl_config.h`

**Interfaces:**
- Consumes: MSPM0 SDK `C:\ti\ti\mspm0_sdk_2_11_00_07`，TI Arm Clang 4.0.x。
- Produces: UART1 9600 baud，RX 中断和 TX 中断可由固件显式开关的工程。

- [x] **Step 1:** 提交队友原始副本基线，排除 `Debug/`。
- [x] **Step 2:** 把 `.cproject/.ccsproject` 中队友的绝对 SDK 路径更换为本机 SDK 2.11，不手改生成 makefile。
- [x] **Step 3:** 将 `UART_DEBUG.targetBaudRate` 改为 `9600`，用 SysConfig CLI 重生成文件。
- [x] **Step 4:** 用 TI Arm Clang 严格 clean build；期望链接成功且无 error。CCS 图形界面导入和烧录不属于本次验收条件。
- [x] **Step 5:** 提交 `chore: make teammate project reproducible locally`。

## Task 2: 协议纯逻辑与测试向量

**Files:**
- Create: `inc/crc16.h`, `src/crc16.c`, `inc/ring_buffer.h`, `src/ring_buffer.c`
- Create: `python/autotune/protocol.py`, `python/tests/test_protocol.py`

**Interfaces:**
- Produces: `uint16_t CRC16_Calculate(const uint8_t *, uint16_t)`；`RingBuffer_PushFromISR/Pop/Count/Free/IsEmpty`；Python `encode_frame()` 和 `FrameDecoder.feed()`。
- Frame: `A5 5A | version:u8 | type:u8 | flags:u8 | seq:u16le | length:u16le | payload | crc:u16le`，CRC 覆盖 version 至 payload。

- [x] **Step 1 (RED):** 在 `test_protocol.py` 写入 `123456789 -> 0x29B1`、粘包、截断、噪声、CRC 错误和 128 B 上限测试，运行 `E:\python\python.exe -m unittest discover -s python/tests -v`，期望因模块不存在失败。
- [x] **Step 2 (GREEN):** 实现 Python CRC/编解码，只让上述测试通过。
- [x] **Step 3 (RED):** 添加环形队列满/空/回绕向量和 C/Python 共享帧的 golden vectors。
- [x] **Step 4 (GREEN):** 实现 C CRC 与环形队列，并用 TI Arm Clang 严格编译。
- [x] **Step 5:** 提交 `feat: add protocol primitives and golden vectors`。

## Task 3: UART 非阻塞搬运与 HELLO

**Files:**
- Create: `inc/uart_transport.h`, `src/uart_transport.c`, `inc/protocol.h`, `src/protocol.c`, `inc/app_protocol.h`, `src/app_protocol.c`
- Modify: `src/main.c`, `empty.syscfg`
- Test: `python/tests/test_protocol.py`, `python/tests/test_link.py`

**Interfaces:**
- Produces: `UART_Transport_Init/Write/ReadByte/GetCounters`；`Protocol_Poll/Send`；`AppProtocol_HandleFrame`。
- HELLO ACK payload 固定包含 protocol=1、firmware semantic version、capability bits、boot session ID。

- [x] **Step 1 (RED):** 添加 HELLO payload 往返和未知版本 NACK 测试，确认失败原因是业务编解码尚未存在。
- [x] **Step 2 (GREEN):** UART ISR 只搬字节，主循环流式解码；TX 满时返回 false，不等待。
- [x] **Step 3:** 构建固件，确认 `UART1_IRQHandler` 唯一且无符号冲突。
- [x] **Step 4 (hardware gate，本次验收豁免):** 不接电机运行，用 JDY-31 9600 8N1 发 HELLO，确认匹配 ACK；本次不烧录、不连接实物，保留到以后按硬件清单执行。
- [x] **Step 5:** 提交 `feat: add nonblocking bluetooth protocol link`。

## Task 4: 参数、单调调度、状态机与统一停车

**Files:**
- Create: `inc/autotune_types.h`, `inc/control_params.h`, `src/control_params.c`, `inc/safety_guard.h`, `src/safety_guard.c`, `inc/trial_manager.h`, `src/trial_manager.c`
- Modify: `inc/main.h`, `src/main.c`, `inc/motor.h`, `src/motor.c`, `src/app_protocol.c`
- Test: `python/tests/test_models.py`, `python/tests/test_session.py`, `python/tests/test_trial.py`, `tests/firmware_host/test_core.c`

**Interfaces:**
- Produces: `ControlParams_Stage/ApplyPendingAtBoundary/GetActive`；`SafetyGuard_Stop(reason,fault,tick)`；`TrialManager_Arm/Start/Abort/ClearFault`；`gControlTick`。
- `Motor_Stop()` 同一次调用清零两 PWM 比较值和四方向脚。

- [x] **Step 1:** 对参数越界、RUNNING 拒绝、非 RUNNING 边界整组切换和版本递增完成源码审计与严格编译；`test_models.py` 覆盖线格式和安全默认值。本次未运行 ARM 目标行为测试，不把它写成已运行。
- [x] **Step 2 (GREEN):** 实现 Q16.16 参数结构、明确上下限和 pending/active 双缓冲。
- [x] **Step 3:** 对 500 tick 停车和重复 START 去重完成源码审计与严格编译；40 tick 心跳超时有 C 契约向量并完成编译链接，但未烧录运行。
- [x] **Step 4 (GREEN):** 实现七态状态机、Sequence+Session 去重和统一停车。
- [x] **Step 5:** 用 `volatile uint32_t gControlTick` 替代布尔 flag；主循环有界追赶并累计 overrun。
- [x] **Step 6:** 构建固件，检查上电路径在任何 I2C/蓝牙操作前执行 `Motor_Stop()`。
- [x] **Step 7:** 提交 `feat: add safe trial state machine`。

## Task 5: 速度 PI、循线 PD 与本地联锁

**Files:**
- Create: `inc/speed_pi.h`, `src/speed_pi.c`, `inc/line_control.h`, `src/line_control.c`
- Modify: `inc/gray_sensor.h`, `src/gray_sensor.c`, `inc/encoder.h`, `src/encoder.c`, `src/safety_guard.c`, `src/trial_manager.c`, `src/main.c`
- Retire: `inc/pid.h`, `src/pid.c` 不再进入构建
- Test: `tests/firmware_host/test_core.c`

**Interfaces:**
- Produces: `SpeedPI_Step/Reset`；`LineControl_Step`；`LineSensorSample {bitmap,error,activeCount,valid}`；安全计时器。

- [x] **Step 1:** 写 PI 限幅、条件积分、目标为零清积分和独立状态向量；纳入严格交叉编译。本次不烧录，因此不声称 ARM 目标测试已运行。
- [x] **Step 2 (GREEN):** 使用 64 位中间量实现 Q16.16 PI/前馈和饱和运算。
- [x] **Step 3:** 写无效灰度、D 限幅/低通和目标速度限幅契约向量；纳入严格交叉编译。
- [x] **Step 4 (GREEN):** 实现外环 PD，不再用旧误差掩盖丢线。
- [x] **Step 5:** 以 10 ms 样本编写连续 15 个无线样本、30 个堵转样本和限定 overrun 的本地停车契约向量；纳入严格交叉编译，实物触发测试本次豁免。
- [x] **Step 6 (hardware gate，本次验收豁免):** 车轮悬空，PWM 上限不高于 25%，确认编码器方向、左右电机对应和停车脚电平；本次不烧录、不运行电机，保留到以后按硬件清单执行。
- [x] **Step 7:** 提交 `feat: add cascaded line and wheel control`。

## Task 6: 本地统计、遥测和可靠汇总

**Files:**
- Create: `inc/trial_stats.h`, `src/trial_stats.c`
- Modify: `inc/autotune_types.h`, `src/trial_manager.c`, `src/app_protocol.c`
- Test: `python/tests/test_models.py`, `tests/firmware_host/test_core.c`

**Interfaces:**
- Produces: `TrialStats_Reset/AddSample/Finish`；`TRIAL_SUMMARY`；10 Hz `TELEMETRY`。

- [x] **Step 1:** 用确定样本编写 IAE、平方和、max、P95 直方图、丢线连续值、符号翻转和饱和计数契约向量；纳入严格交叉编译。
- [x] **Step 2 (GREEN):** 实现 64 位饱和累加，不依赖遥测发送成功。
- [x] **Step 3:** 汇总帧用高优先级可靠队列；低优先级遥测在 TX 不足时计数并丢弃。
- [x] **Step 4:** 完整构建并运行 Python 解码测试。
- [x] **Step 5:** 提交 `feat: add local trial statistics and telemetry`。

## Task 7: Python 会话、试验、评分与有界优化

**Files:**
- Create: `python/autotune/link.py`, `session.py`, `trial.py`, `scoring.py`, `optimizer.py`, `config.py`, `cli.py`, `__main__.py`
- Create: `python/tests/test_link.py`, `test_session.py`, `test_trial.py`, `test_scoring.py`, `test_optimizer.py`
- Create: `python/requirements.txt`, `python/config.example.json`

**Interfaces:**
- Produces: `SerialLink.request()`；`AutotuneSession`；`TrialRunner.run_candidate()`；`score_summary()`；`BoundedCoordinateSearch`。

- [x] **Step 1 (RED):** 用内存 fake transport 测试 ACK seq 匹配、超时重试上限、100 ms 心跳和链路关闭停止心跳。
- [x] **Step 2 (GREEN):** 实现串口抽象和会话，串口只被一个 Python 进程打开。
- [x] **Step 3 (RED/GREEN):** 验证严格 SET_PARAMS ACK→ARM ACK→START ACK→SUMMARY 顺序，任一失败暂停且 last-safe 不被覆盖。
- [x] **Step 4 (RED/GREEN):** 测试正常分数可离线复算，任一安全故障的惩罚大于最大速度奖励。
- [x] **Step 5 (RED/GREEN):** 测试网格/坐标搜索不越界、失败候选不成为 best、随时可返回 last-safe。
- [x] **Step 6:** CLI 提供 `ports`, `hello`, `status`, `abort`, `clear`, `trial`, `identify`, `tune-speed`, `tune-line`, `climb`, `commit`；默认不自动启动电机。
- [x] **Step 7:** 原始帧、JSONL 事件、CSV 汇总和评分配置同时保存。
- [x] **Step 8:** 提交 `feat: add safe Python autotuning workflow`。

## Task 8: Flash 双槽最优参数

**Files:**
- Create: `inc/flash_params.h`, `src/flash_params.c`
- Modify: linker command/config as required, `src/trial_manager.c`, `src/app_protocol.c`, `src/main.c`
- Test: `python/tests/test_flash_record.py`

**Interfaces:**
- Produces: `FlashParams_LoadLatest/Commit`；记录 `{magic,schema,generation,length,params,crc}`。

- [x] **Step 1 (RED):** 在存储后端抽象上测试双无效用安全默认、新 generation 胜出、新槽损坏回退旧槽和同版本不重复写。
- [x] **Step 2 (GREEN):** 实现可测的记录选择逻辑，再接 DriverLib erase/program/readback。
- [x] **Step 3:** 只允许 IDLE→FLASH_WRITE，写前统一停车，校验成功回 IDLE，失败进 FAULT。
- [x] **Step 4:** 构建并检查 linker map，确认两槽不与程序段重叠。
- [x] **Step 5:** 提交 `feat: persist explicitly committed parameters`。

## Task 9: VOFA+ 转发、文档与最终验证

**Files:**
- Create: `python/autotune/vofa.py`, `python/tests/test_vofa.py`
- Modify: `README.md`, `python/config.example.json`
- Create: `docs/protocol.md`, `docs/hardware-test-checklist.md`, `docs/deviations.md`

**Interfaces:**
- Produces: VOFA+ FireWater 可选输出；可执行的蓝牙、悬空、低速和 Flash 验收清单。

- [x] **Step 1 (RED/GREEN):** 测试 VOFA 未启动、连接中断或发送失败时只记转发丢弃，不向 trial 抛出异常。
- [x] **Step 2:** 文档写明 JDY-31 四线接法、9600 8N1、Python 独占 COM、VOFA+ 连 Python 本机转发口。
- [x] **Step 3:** 运行全部 Python 测试，期望 0 failures/0 errors。
- [x] **Step 4:** SysConfig 重生成后 clean build 固件，期望 exit 0 且链接 map 无槽位冲突。
- [x] **Step 5:** 逐条对照 `1-3.md` 第 17 节；只能由使用者在实物上完成的项目明确标为 `WAITING_FOR_HARDWARE_TEST`，并注明本次验收豁免，不伪称实测通过。
- [x] **Step 6:** 提交 `docs: add bring-up and acceptance guide`。

## 计划自检结论

- 覆盖 `1-3` 第 6—17 节的所有首版实现要求；第 18 节的赛道数据保持为现场标定输入。
- 未将电池 ADC、电流检测、直角专用状态机或 MPU6050 加入首版必需链路。
- 修正了 `PA25` 实际已被灰度 D6 占用的冲突，不改 PCB，不实现电池采样。
- PC 可验证范围由 Python 协议/模型测试、SysConfig 重生成、C 严格交叉编译链接和 Flash map 检查覆盖。硬件 ISR/DriverLib 的实物行为没有被虚称为已验证，本次按用户要求豁免，留待以后执行硬件清单。
