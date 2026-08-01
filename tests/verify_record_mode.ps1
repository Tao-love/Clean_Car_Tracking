$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Header = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'inc\record.h')
$Source = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\record.c')
$Main = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\main.c')
$Uart = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\UART_Auto_PID.c')

foreach ($api in @(
    'void Record_Init(void);', 'bool Record_Start(void);',
    'bool Record_Stop(void);', 'bool Record_IsRecording(void);',
    'int32_t Record_GetLeft(void);', 'int32_t Record_GetRight(void);'
)) {
    if (-not $Header.Contains($api)) { throw "record API 缺失: $api" }
}
if ($Source -notmatch 'Encoder_GetLeftCount\(\)' -or
    $Source -notmatch 'Encoder_GetRightCount\(\)' -or
    $Source -notmatch 'gRecordLeft' -or $Source -notmatch 'gRecordRight') {
    throw 'record 模块必须保存左右编码器增量。'
}
if ($Main -notmatch 'GPIO_KEYS_KEY4_PORT' -or
    $Main -notmatch 'Record_Start\(\)' -or
    $Main -notmatch 'Record_Stop\(\)') {
    throw 'KEY4 必须切换记录模式，且不得调用 LineRun 启动 API。'
}
if ($Uart -notmatch 'Record_GetLeft\(\)' -or
    $Uart -notmatch 'Record_GetRight\(\)' -or
    $Uart -notmatch 'ACK,') {
    throw 'SETALL 成功回包必须附加左右记录值。'
}
Write-Host 'Encoder record mode contract passed.'
