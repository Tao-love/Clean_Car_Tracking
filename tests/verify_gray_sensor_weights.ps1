<# 八路灰度权重的实车调参契约。 #>
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Source = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\gray_sensor.c')

if ($Source -notmatch '-3500, -2500, -2200, -500, 500, 2200, 2500, 3500') {
    throw '灰度权重必须恢复为初版实车标定值。'
}

Write-Host 'Gray-sensor weight contract passed.'
