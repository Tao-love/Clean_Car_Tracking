param(
    [string]$HeaderPath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'syscfg_gen\ti_msp_dl_config.h')
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$required = @(
    '#define UART_DEBUG_INST UART2',
    '#define GPIO_UART_DEBUG_RX_PORT GPIOB',
    '#define GPIO_UART_DEBUG_TX_PORT GPIOB',
    '#define GPIO_UART_DEBUG_RX_PIN DL_GPIO_PIN_16',
    '#define GPIO_UART_DEBUG_TX_PIN DL_GPIO_PIN_15',
    '#define GPIO_UART_DEBUG_IOMUX_RX_FUNC IOMUX_PINCM33_PF_UART2_RX',
    '#define GPIO_UART_DEBUG_IOMUX_TX_FUNC IOMUX_PINCM32_PF_UART2_TX',
    '#define UART_DEBUG_BAUD_RATE (9600)'
)
$text = Get-Content -Raw -LiteralPath $HeaderPath
$normalized = $text -replace '\s+', ' '
$missing = $required | Where-Object { -not $normalized.Contains($_) }
if ($missing.Count -ne 0) {
    $missing | ForEach-Object { Write-Error "Missing: $_" }
    exit 1
}

$Syscfg = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'empty.syscfg')
$Main = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\main.c')
if ($Syscfg -match 'enabledInterrupts|rxFifoThreshold|/ti/driverlib/TRNG') {
    throw 'UART2 只能保留 9600 8N1 与 PB15/PB16 基础配置，且必须删除 TRNG。'
}
if ($Main -match 'UART_Transport|UART_Echo|Protocol|UART_DEBUG') {
    throw '运行入口不得初始化、轮询或发送 UART 数据。'
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

Write-Host 'UART2 basic-only SysConfig and no-runtime contract passed.'
