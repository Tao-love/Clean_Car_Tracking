# basic-1 实物验收清单

下列项目必须按顺序执行。当前未由本机实物验证的项目统一标为 `WAITING_FOR_HARDWARE_TEST`，不能因为软件测试通过就改成已通过。

## 0. 准备

- [ ] `WAITING_FOR_HARDWARE_TEST` 电池插头可以立即拔除。
- [ ] `WAITING_FOR_HARDWARE_TEST` 首次电机试验车轮悬空。
- [ ] `WAITING_FOR_HARDWARE_TEST` JDY-31 使用稳定 5 V，UART 为 3.3 V 逻辑并共地。
- [ ] `WAITING_FOR_HARDWARE_TEST` 确认 STBY 固定 5 V，测试人员理解软件停车不等于物理断电。

## 1. 无电机通信

- [ ] `WAITING_FOR_HARDWARE_TEST` PA8/TX1→RXD、PA9/RX1←TXD、GND、5V 四线检查。
- [ ] `WAITING_FOR_HARDWARE_TEST` `python -m autotune --port COMx hello` 返回协议 1、固件版本和非零 Session ID。
- [ ] `WAITING_FOR_HARDWARE_TEST` 连续 status/heartbeat 无 CRC、长度或 RX overflow 增长。
- [ ] `WAITING_FOR_HARDWARE_TEST` 随机噪声、截断、粘包和 CRC 错误不执行命令。

## 2. 停车与状态机

- [ ] `WAITING_FOR_HARDWARE_TEST` 上电后 PWMA/PWMB 为 0，AIN1/AIN2/BIN1/BIN2 全低。
- [ ] `WAITING_FOR_HARDWARE_TEST` RUNNING 断开蓝牙后不晚于 400 ms 统一停车并锁存 `COMM_TIMEOUT`。
- [ ] `WAITING_FOR_HARDWARE_TEST` 每次正常 trial 第 500 tick 本地停车，不依赖 Python stop。
- [ ] `WAITING_FOR_HARDWARE_TEST` 重发同一 START Sequence 不重置计时、不重复启动。

## 3. 车轮悬空

- [ ] `WAITING_FOR_HARDWARE_TEST` 以 PWM 50 起步，逐级测试且不超过 400。
- [ ] `WAITING_FOR_HARDWARE_TEST` 正 PWM 时左右编码器速度符号均为正；不一致时只修改对应 `ENCODER_*_REVERSE` 并重新构建。
- [ ] `WAITING_FOR_HARDWARE_TEST` 左右电机与软件通道对应正确。
- [ ] `WAITING_FOR_HARDWARE_TEST` 实测启动 PWM、空载速度和左右差异，写入独立配置记录。
- [ ] `WAITING_FOR_HARDWARE_TEST` 标定“高 PWM + 低速度”阈值；持续 300 ms 能报告正确车轮并停车。
- [ ] `WAITING_FOR_HARDWARE_TEST` WHEEL_SPEED 阶跃能得到稳定的左右速度 PI 数据。

## 4. 低速循线

- [ ] `WAITING_FOR_HARDWARE_TEST` 核对灰度有效电平和 D0..D7 左右顺序。
- [ ] `WAITING_FOR_HARDWARE_TEST` 首次落地 `max_pwm<=400`、低 baseSpeed，人员可立即拔电池。
- [ ] `WAITING_FOR_HARDWARE_TEST` 八路全未触发持续 150 ms 报 `LINE_LOST` 并停车。
- [ ] `WAITING_FOR_HARDWARE_TEST` RUNNING 中 SET_PARAMS 返回拒绝，参数版本不变。
- [ ] `WAITING_FOR_HARDWARE_TEST` 关闭 VOFA+ 或制造 UDP 转发失败不影响 5 秒停车和汇总。

## 5. Flash

- [ ] `WAITING_FOR_HARDWARE_TEST` 非 IDLE 或版本不匹配时 COMMIT_PARAMS 被拒绝。
- [ ] `WAITING_FOR_HARDWARE_TEST` IDLE 显式提交后复位，加载同一参数版本。
- [ ] `WAITING_FOR_HARDWARE_TEST` 模拟新槽损坏或写入中断电，复位后回退旧槽或保守默认值。
- [ ] `WAITING_FOR_HARDWARE_TEST` 写 Flash 期间 PWM 为 0、四方向脚全低且不能 ARM/START。

## 6. 赛道后加项

- [ ] `WAITING_FOR_TRACK_DATA` 测量轮径、每轮实际计数、线宽、闭环长度、直角位置和起始姿态。
- [ ] `WAITING_FOR_TRACK_DATA` 基础低速循线通过后再实现和验收直角专用状态机。
- [ ] `WAITING_FOR_TRACK_DATA` 确定同一参数的独立 5 秒重复次数和人工复位位置。
