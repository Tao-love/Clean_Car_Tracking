<#
循线差速斜率限制契约。
防止数字灰度跨通道时，左右目标速度在单个 6.667 ms 控制周期内突变。
#>

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Header = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'inc\line_control.h')
$Source = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\line_control.c')

function Require-Match([string]$Text, [string]$Pattern, [string]$Message) {
    if ($Text -notmatch $Pattern) {
        throw $Message
    }
}

Require-Match $Header 'int16_t\s+previousDeltaSpeed;' `
    '循线状态必须保存上一周期的差速。'
Require-Match $Source '#define\s+LINE_DELTA_SLEW_PER_TICK\s+\(6\)' `
    '每个控制周期的差速变化必须限制为 6。'
Require-Match $Source 'delta\s*=\s*LineControl_SlewDelta\(\s*state->previousDeltaSpeed,\s*delta\s*\);' `
    'PD 差速必须在输出前经过斜率限制。'

Write-Host 'Line delta slew contract passed.'
