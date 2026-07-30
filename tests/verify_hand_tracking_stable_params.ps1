$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Source = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\control_params.c')

foreach ($pattern in @(
    '12L\s*\*\s*Q16_ONE,\s*0,\s*42L\s*\*\s*Q16_ONE,',
    '12L\s*\*\s*Q16_ONE,\s*0,\s*\(69L\s*\*\s*Q16_ONE\s*/\s*2\),',
    '\(Q16_ONE\s*/\s*70\),\s*\(Q16_ONE\s*/\s*320\),',
    '\(5L\s*\*\s*Q16_ONE\s*/\s*8\),\s*10000,',
    '10,\s*67,\s*67,',
    '300,\s*2333'
)) {
    if ($Source -notmatch $pattern) { throw "稳定默认参数不匹配: $pattern" }
}
if ($Source -match 'CLEAN_CAR_EXPORTED_') {
    throw '旧自动整定参数宏仍存在。'
}
if ($Source -notmatch 'ControlParams\s*\*ControlParams_GetMutableForUart\(void\)') {
    throw 'UART 可写参数访问接口缺失。'
}
Write-Host 'Hand-tracking stable parameter contract passed.'
