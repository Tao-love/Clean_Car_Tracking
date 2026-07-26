<# 单一循线用户文档契约。 #>
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Readme = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'README.md')

foreach ($pattern in @(
    '^# Clean_Car', '150 Hz', '1500', '10 秒',
    'src/line_run\.c', 'src/control_params\.c',
    '最后一次有效误差', 'XDS110', 'Clean_Car\.out',
    'clean-car-working-line-baseline', 'clean-car-line-only-v1'
)) {
    if ($Readme -notmatch $pattern) {
        throw "README 缺少单一循线说明: $pattern"
    }
}
if ($Readme -match 'UART2 READY|原样回显|UART BSL|CH340|FAULT 锁存|5 秒|15 秒') {
    throw 'README 仍包含已经删除的运行方式或错误时长。'
}

Write-Host 'Line-follow-only README contract passed.'
