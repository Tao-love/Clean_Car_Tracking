$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$source = Get-Content -Raw -LiteralPath (Join-Path $projectRoot 'src\control_params.c')

if ($source -notmatch '\(5L \* Q16_ONE / 8\), 10000,\s*10, 67, 67,') {
    throw '默认 baseSpeed 必须为 10，其他固定控制限制必须保持不变。'
}

Write-Host 'baseSpeed=10 contract passed.'
