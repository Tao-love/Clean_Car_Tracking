# 1_USB UART BSL 直烧配置 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** 让 1_USB 通过板载 CH340（当前为 COM14）和 MSPM0G3507 UART BSL 下载，并生成备用 Intel HEX 固件。

**Architecture:** 固件源码不改变；CCS 工程元数据使用 UART Connection，构建输出 ELF .out。独立 PowerShell 工具从 .out 生成 Intel HEX，README 清晰区分 COM14/BSL 与 XDS110 调试。

**Tech Stack:** CCS 20.7.0 Theia、MSPM0 SDK 2.11.00.07、TI Arm Clang 4.0.2 LTS、PowerShell 5+。

## Global Constraints

- 只修改 E:\TI_work\TI_Project\1_USB；绝不修改 basic-1。
- 下载口为 USB-SERIAL CH340 (COM14)；蓝牙 COM 口不得用于烧录。
- UART BSL 不是 XDS110：不得宣称支持 SWD 断点、在线内存读取或单步。
- 不改动 src/、inc/、empty.syscfg、PID、安全联锁或蓝牙协议。
- 新增代码使用中文模块化注释；新增代码块按项目约定以 ----- AI 注释线隔开。
- 未获得板子的 BOOT/RESET 时序前，README 不臆造进入 BSL 的按键顺序。

---

### Task 1: 固化 UART BSL 工程入口

**Files:**
- Modify: .ccsproject:5-17
- Modify: targetConfigs/MSPM0G3507.ccxml:1-14
- Modify: .theia/launch.json:1-25
- Modify: targetConfigs/readme.txt:1-8

**Interfaces:**
- Consumes: USB-SERIAL CH340 (COM14) 和 MSPM0G3507 ROM UART BSL。
- Produces: 唯一使用 UARTConnection 的目标配置；没有名为 basic-1 的 XDS110 启动配置。创建时保留的 projectspec.basic-1 模板标识不参与构建或下载，必须保留。

- [x] **Step 1: 验证旧配置存在**

Run:

~~~powershell
rg -n -S 'Texas Instruments XDS110|basic-1|UARTConnection|MSPM0G3507' .ccsproject targetConfigs .theia
~~~

Expected: .theia/launch.json 显示 basic-1 和 XDS110，targetConfigs/MSPM0G3507.ccxml 显示 UARTConnection 和 MSPM0G3507；.ccsproject 的 projectspec.basic-1 是无运行效果的历史模板标识。

- [x] **Step 2: 写入最小元数据修改**

在 .ccsproject 保留并启用：

~~~xml
<connection value="common/targetdb/connections/uart_connection.xml"/>
<activeTargetConfiguration value="targetConfigs/MSPM0G3507.ccxml"/>
<isTargetConfigurationManual value="true"/>
~~~

保持 .ccxml 内以下节点：

~~~xml
<instance desc="UARTConnection" href="connections/uart_connection.xml" id="UARTConnection" xml="uart_connection.xml" xmlpath="connections"/>
<instance desc="MSPM0G3507" href="devices/MSPM0G3507.xml" id="MSPM0G3507" xml="MSPM0G3507.xml" xmlpath="devices"/>
~~~

删除 .theia/launch.json 中固定 basic-1 / Texas Instruments XDS110 USB Debug Probe 的 launch 条目；不凭空编造 Theia UART 启动 JSON。于 targetConfigs/readme.txt 首段写明：该 .ccxml 是 CH340 UART BSL 目标，CCS 中应选 COM14，不选蓝牙或 XDS110。

- [x] **Step 3: 验证元数据**

Run:

~~~powershell
rg -n -S 'Texas Instruments XDS110|basic-1' .theia
rg -n -S 'UARTConnection|MSPM0G3507|isTargetConfigurationManual value="true"' .ccsproject targetConfigs/MSPM0G3507.ccxml
~~~

Expected: 第一条无结果（退出码 1），第二条显示三项要求；.ccsproject 中的 projectspec.basic-1 保持不变。

- [x] **Step 4: Commit**

~~~powershell
git add .ccsproject targetConfigs/MSPM0G3507.ccxml targetConfigs/readme.txt .theia/launch.json
git commit -m "config: select CH340 UART BSL target"
~~~

### Task 1A: 修复 CCS 生产固件的源文件筛选

**Files:**
- Modify: .cproject:109

**Interfaces:**
- Consumes: SysConfig 自动提供的 TI 启动文件与 device_linker.cmd。
- Produces: CCS Debug 链接时只使用 TI 格式 .cmd、一个启动向量表和正式固件源；tests/firmware_host 不进入可烧录固件。

- [x] **Step 1: 复现生成的 Debug 链接错误**

Run:

~~~powershell
& 'C:\ti\ccs2100\ccs\utils\bin\gmake.exe' -C Debug -j 4 all
~~~

Expected: 未排除时会出现 REGION_ALIAS（.lds 被 TI 链接器读取）或 main / interruptVectors 重定义（测试或重复启动文件被链接）。

- [x] **Step 2: 写入唯一的永久筛选规则**

将 sourcePath 的 excluding 属性设为：

~~~xml
<entry excluding="syscfg_gen/device_linker.cmd|syscfg_gen/device_linker.lds|src/startup_mspm0g350x_ticlang.c|tests" flags="VALUE_WORKSPACE_PATH|RESOLVED" kind="sourcePath" name=""/>
~~~

含义：SysConfig 自己将 device_linker.cmd 与 TI 官方启动文件加入构建；不能再作为普通工程源重复加入 GNU .lds、项目内启动文件或离线 test_core。

- [x] **Step 3: 验证生成命令的源集合**

在 CCS 执行 Project → Clean 后执行 Project → Build Project。终端链接命令必须包含 syscfg_gen/device_linker.cmd，且不含 device_linker.lds、src/startup_mspm0g350x_ticlang.o、tests/firmware_host/test_core.o；应生成 Debug/1_USB.out。

- [x] **Step 4: Commit**

~~~powershell
git add .cproject
git commit -m "fix: exclude test and duplicate linker inputs"
~~~

### Task 2: 增加 ELF 到 Intel HEX 的导出工具

**Files:**
- Create: tools/export_usb_hex.ps1
- Modify: .gitignore:1-10

**Interfaces:**
- Consumes: Debug/1_USB.out 与 C:\Ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmobjcopy.exe。
- Produces: Debug/1_USB.hex；失败时返回非零退出码。

- [x] **Step 1: 证明脚本尚不存在**

Run:

~~~powershell
powershell -ExecutionPolicy Bypass -File tools\export_usb_hex.ps1
~~~

Expected: 报“找不到路径或文件”。

- [x] **Step 2: 编写最小导出工具**

创建 tools/export_usb_hex.ps1：

~~~powershell
# ----- AI
# 将 CCS 的 MSPM0 ELF 固件转换为 Intel HEX，供 UART BSL 备用下载器使用。
param(
    [string]$InputElf = (Join-Path $PSScriptRoot '..\Debug\1_USB.out'),
    [string]$OutputHex = (Join-Path $PSScriptRoot '..\Debug\1_USB.hex'),
    [string]$Objcopy = 'C:\Ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmobjcopy.exe'
)

if (-not (Test-Path -LiteralPath $InputElf)) { throw "未找到已构建的固件：$InputElf；请先在 CCS 执行 Build Project。" }
if (-not (Test-Path -LiteralPath $Objcopy)) { throw "未找到 TI objcopy：$Objcopy" }
& $Objcopy -O ihex $InputElf $OutputHex
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $OutputHex)) { throw 'Intel HEX 导出失败。' }
Write-Host "已生成：$OutputHex"
# ----- AI
~~~

Debug/ 已被 .gitignore 忽略，不能提交 .hex。

- [x] **Step 3: 构建并验证**

在 CCS 执行 Project → Build Project 后运行：

~~~powershell
powershell -ExecutionPolicy Bypass -File tools\export_usb_hex.ps1
Get-Item Debug\1_USB.out, Debug\1_USB.hex | Select-Object Name,Length
~~~

Expected: 两项长度均大于 0，且脚本打印 已生成：。out 缺失时先修复构建，不能使用旧的 basic-1.out。

- [x] **Step 4: Commit**

~~~powershell
git add tools/export_usb_hex.ps1 .gitignore
git commit -m "tools: add Intel HEX export for UART BSL"
~~~

### Task 3: 编写 CCS 下载说明

**Files:**
- Modify: README.md:1-100

**Interfaces:**
- Consumes: .ccxml、Debug/1_USB.out、Debug/1_USB.hex。
- Produces: 操作者可识别 COM14、选择 UART BSL 并在实物下载前执行安全准备。

- [x] **Step 1: 验证文档缺失项**

Run:

~~~powershell
rg -n -S 'COM14|CH340|UART BSL|1_USB.out|1_USB.hex|XDS110' README.md
~~~

Expected: 当前文档未完整覆盖 CH340 下载流程。

- [x] **Step 2: 增加“USB 直烧（CH340 UART BSL）”章节**

在“电脑环境”前写入以下可操作内容：

~~~markdown
## USB 直烧（CH340 UART BSL）

板载 USB 转串口在本电脑当前显示为 USB-SERIAL CH340 (COM14)；它是下载口，JDY-31 的蓝牙 COM 口不是下载口。

1. 车轮悬空或断开电机主电源，连接普通 USB 数据线。
2. 在设备管理器确认 USB-SERIAL CH340 (COM14)；COM 号变动时以当前 CH340 项为准。
3. 在 CCS 导入并选择 1_USB，执行 Project → Build Project，确认生成 Debug/1_USB.out。
4. 在 CCS 选择 targetConfigs/MSPM0G3507.ccxml 的 UARTConnection，按客服提供的 BOOT/RESET 时序进入 BSL，再选择 CH340 的 COM14 下载。
5. 若下载窗口需要 Intel HEX，运行 powershell -ExecutionPolicy Bypass -File tools\export_usb_hex.ps1，选择 Debug/1_USB.hex。

UART BSL 仅用于下载；它不能像 XDS110 一样提供断点、单步和实时内存查看。若连接失败，先检查 USB 数据线、COM 端口占用和客服给出的 BOOT/RESET 时序。
~~~

- [x] **Step 3: 验证说明一致**

Run:

~~~powershell
rg -n -S 'COM14|CH340|UART BSL|Debug/1_USB.out|Debug/1_USB.hex' README.md
git diff --check
git status --short
~~~

Expected: 文档含全部关键标识；git diff --check 无输出；状态中不含 Debug/1_USB.out 或 Debug/1_USB.hex。

- [x] **Step 4: Commit**

~~~powershell
git add README.md
git commit -m "docs: document CH340 UART BSL flashing"
~~~

### Task 4: 最终离线验收

**Files:**
- Modify: tools/verify.ps1:1-120

**Interfaces:**
- Consumes: 现有固件源码、SysConfig、Python 测试与工程名。
- Produces: .build/verify/1_USB.out 的构建/链接验证；不执行硬件烧录。

- [x] **Step 1: 确认旧工程名仍在脚本中**

Run:

~~~powershell
rg -n -S 'basic-1' tools\verify.ps1
~~~

Expected: 标题、basic-1.map 和 basic-1.out。

- [x] **Step 2: 对齐工程名**

将标题改为 1_USB 完整离线验证，并将两行改为：

~~~powershell
$Map = Join-Path $Output '1_USB.map'
$Firmware = Join-Path $Output '1_USB.out'
~~~

不得修改编译参数、链接地址、测试范围或源文件列表。

- [x] **Step 3: 执行验证**

Run:

~~~powershell
powershell -ExecutionPolicy Bypass -File tools\verify.ps1
Test-Path .build\verify\1_USB.out
~~~

Expected: 脚本退出码为 0，最后输出 True。这只证明离线构建和测试通过，不证明已写入实物板。

- [x] **Step 4: Commit**

~~~powershell
git add tools/verify.ps1
git commit -m "test: align offline verification with 1_USB"
~~~

## 实物操作验收

1. 关闭可能占用 COM14 的 CCS、串口助手或 Python 程序，仅保留一个下载程序。
2. 轮子悬空或断开电机主电源。
3. 根据客服给出的精确 BOOT/RESET 时序进入 BSL；该时序未确认前不随机按键。
4. 在 CCS UART BSL 下载界面选择 CH340 / COM14 和 Debug/1_USB.out；若只接受 HEX 则选择 Debug/1_USB.hex。
5. 下载成功后复位，再按蓝牙通信测试流程验证应用功能。
