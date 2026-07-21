# 1_USB：CH340 UART BSL 直烧配置设计

## 目标

将 `E:\TI_work\TI_Project\1_USB` 保持为 `basic-1` 的独立副本，并使其可经板载 CH340（设备管理器中为 `USB-SERIAL CH340 (COM14)`）和普通 USB 数据线下载到 MSPM0G3507。

本设计只处理固件构建和下载入口；不改变循线、蓝牙、PID、安全状态机或现有 PCB 接线。

## 已知条件

- 目标 MCU 为 MSPM0G3507。
- 板载 CH340 已被 Windows 识别为 `COM14`。
- 客服确认该板支持通过普通 USB 数据线烧录。
- 该路径应使用 MSPM0 的 UART BSL；CH340 不是 XDS110，不能提供 SWD 单步调试或断点功能。

## 方案选择

采用 CCS 的 UART 连接配置作为下载入口，而不是 XDS110 连接配置。该方式与硬件相符，且允许在 CCS 内继续构建并由 UART BSL 写入程序。

保留 Intel HEX 导出作为备用：若 CCS 的 UART BSL 下载窗口只接受 HEX，或客服提供独立 BSL 下载器，可使用同一构建产生的 HEX 文件。ELF `.out` 仍是 CCS 的编译和调试信息文件。

## 工程修改

1. `targetConfigs/MSPM0G3507.ccxml` 保持 UART Connection，并增加面向 CH340 / COM14 的使用说明；串口号不硬编码到仓库配置，避免 Windows 改号后工程失效。
2. 修正工程名称和构建输出：从 `basic-1` 统一为 `1_USB`，使默认输出为 `Debug/1_USB.out`，杜绝 CCS 查找旧的 `basic-1.out`。
3. 启用或补足 HEX 输出，使构建后获得 `Debug/1_USB.hex`（若 TI 工具链实际输出路径有差异，README 写明实测路径）。
4. 更新 README，给出明确流程：确认 COM14 → Build Project → 让板子进入客服规定的 BSL 模式 → 以 UART Connection 发起下载。由于尚未获得板子 BOOT/RESET 丝印或客服按键时序，进入 BSL 的精确按键步骤不在本次工程中臆造。

## 错误处理与安全

- 过去的 `Trouble Reading Memory` / GEL 报错不能被当作程序错误；UART BSL 不是在线调试器，下载时不使用断点、内存查看或普通 SWD 调试流程。
- 下载前让车轮悬空或断开电机主电源。烧录完成首次复位后，固件虽默认不运行试验，但硬件 STBY 固定为高电平，仍不应在地面上首次试跑。
- 若 COM14 消失或更换为其他 COM 号，以设备管理器当前的 `USB-SERIAL CH340` 端口为准。
- 若无法连接，优先核对 USB 线、COM 端口占用和客服的 BSL 进入时序；不修改 PID 源码来解决下载连接问题。

## 验收

1. `1_USB` 可成功 Build，生成名称为 `1_USB` 的 `.out`，并生成 HEX 备用文件。
2. CCS 的目标配置明确是 UART Connection，不再选 XDS110。
3. README 可让操作者识别 COM14、知道下载与调试的边界，并避免把蓝牙 COM 误选为下载口。
4. 最终实际验收以 CH340/COM14 在 BSL 模式下成功写入并复位启动为准；该操作需要用户在实物板上配合执行。
