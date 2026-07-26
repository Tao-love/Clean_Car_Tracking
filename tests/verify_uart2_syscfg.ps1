param(
    [string]$HeaderPath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'syscfg_gen\ti_msp_dl_config.h')
)

$required = @(
    '#define UART_DEBUG_INST UART2',
    '#define UART_DEBUG_INST_IRQHandler UART2_IRQHandler',
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
Write-Host 'UART2 PB15/PB16 SysConfig contract passed.'
