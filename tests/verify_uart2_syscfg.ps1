param(
    [string]$HeaderPath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'syscfg_gen\ti_msp_dl_config.h')
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$GeneratedHeader = Get-Content -Raw -LiteralPath $HeaderPath
$normalizedHeader = $GeneratedHeader -replace '\s+', ' '
$requiredGenerated = @(
    '#define UART_DEBUG_INST UART2',
    '#define GPIO_UART_DEBUG_RX_PORT GPIOB',
    '#define GPIO_UART_DEBUG_TX_PORT GPIOB',
    '#define GPIO_UART_DEBUG_RX_PIN DL_GPIO_PIN_16',
    '#define GPIO_UART_DEBUG_TX_PIN DL_GPIO_PIN_15',
    '#define GPIO_UART_DEBUG_IOMUX_RX_FUNC IOMUX_PINCM33_PF_UART2_RX',
    '#define GPIO_UART_DEBUG_IOMUX_TX_FUNC IOMUX_PINCM32_PF_UART2_TX',
    '#define UART_DEBUG_BAUD_RATE (115200)'
)
$missing = $requiredGenerated | Where-Object { -not $normalizedHeader.Contains($_) }
if ($missing.Count -ne 0) {
    $missing | ForEach-Object { Write-Error "Missing generated UART2 contract: $_" }
    exit 1
}

$Syscfg = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'empty.syscfg')
$Header = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'inc\UART_Auto_PID.h')
$Source = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\UART_Auto_PID.c')
$LineRun = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\line_run.c')
$Main = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\main.c')

if ($Syscfg -match 'enabledInterrupts|rxFifoThreshold|/ti/driverlib/TRNG') {
    throw 'UART2 必须保持轮询式 115200 8N1 配置，且不能引入 UART 中断或 TRNG。'
}
if ($Syscfg -notmatch '\.targetBaudRate\s*=\s*115200;' -or
    $Syscfg -notmatch '\.peripheral\.\$assign\s*=\s*"UART2";' -or
    $Syscfg -notmatch '\.peripheral\.rxPin\.\$assign\s*=\s*"PB16";' -or
    $Syscfg -notmatch '\.peripheral\.txPin\.\$assign\s*=\s*"PB15";') {
    throw 'empty.syscfg 必须把 UART2 配置为 PB15 TX / PB16 RX / 115200。'
}

foreach ($api in @(
    'void UART_Auto_PID_Service(void);',
    '#define UART_AUTO_PID_TELEMETRY_COLUMNS (21U)'
)) {
    if (-not $Header.Contains($api)) {
        throw "UART 自动调参头文件契约缺失: $api"
    }
}
foreach ($token in @(
    'DL_UART_Main_isRXFIFOEmpty', 'DL_UART_Main_receiveData',
    'DL_UART_Main_isTXFIFOFull', 'DL_UART_Main_transmitData',
    'UART_DEBUG_INST', 'UART_Auto_PID_FormatTelemetry',
    'UART_AUTO_PID_TELEMETRY_COLUMNS', 'UART_Auto_PID_Checksum',
    'UART_Auto_PID_ProtectFrame', 'STATUS', 'SETALL', 'ACK,', 'ERR,'
)) {
    if (-not $Source.Contains($token)) {
        throw "UART2 运行时/协议契约缺失: $token"
    }
}
if ($Source -match 'DL_UART_Main_(transmitDataBlocking|receiveDataBlocking)') {
    throw 'UART2 服务不得使用阻塞式 DriverLib 收发。'
}
if ($LineRun -match 'DL_UART|UART_DEBUG|UART_Auto_PID_Service|UART_Auto_PID_FormatTelemetry') {
    throw 'LineRun_ControlTick 所在模块不得轮询、格式化或收发 UART。'
}
if ($Main -notmatch 'while\s*\(1\)[\s\S]*UART_Auto_PID_Service\(\);') {
    throw 'main 主循环必须持续调用 UART_Auto_PID_Service。'
}
$formatterStart = $Source.LastIndexOf(
    'static bool UART_Auto_PID_FormatTelemetry(char *frame, size_t capacity,')
$queueStart = $Source.IndexOf(
    'static bool UART_Auto_PID_QueueBytes(', $formatterStart)
if (($formatterStart -lt 0) -or ($queueStart -le $formatterStart)) {
    throw '无法定位 21 列遥测格式化函数。'
}
$formatterBody = $Source.Substring($formatterStart, $queueStart - $formatterStart)
$columnCalls = [regex]::Matches(
    $formatterBody, 'UART_Auto_PID_Append(?:Integer|Q16)Column\(').Count
if ($columnCalls -ne 21) {
    throw "遥测格式化器必须按顺序写入 21 列，当前为 $columnCalls 列。"
}

foreach ($obsolete in @(
    'src\app_protocol.c', 'src\crc16.c', 'src\flash_params.c',
    'src\protocol.c', 'src\ring_buffer.c', 'src\uart_echo.c',
    'src\uart_transport.c', 'inc\app_protocol.h', 'inc\crc16.h',
    'inc\flash_params.h', 'inc\protocol.h', 'inc\ring_buffer.h',
    'inc\uart_echo.h', 'inc\uart_transport.h'
)) {
    if (Test-Path -LiteralPath (Join-Path $ProjectRoot $obsolete)) {
        throw "通信调参运行文件仍存在: $obsolete"
    }
}

Write-Host 'UART2 115200 nonblocking runtime and 21-column protocol contract passed.'
