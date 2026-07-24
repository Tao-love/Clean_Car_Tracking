<#
KEY1 手动循线试验时长契约：100 Hz 控制周期下，单次试验必须为 15 秒。
#>

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Header = Get-Content -LiteralPath (Join-Path $ProjectRoot 'inc\trial_manager.h') -Raw
$Source = Get-Content -LiteralPath (Join-Path $ProjectRoot 'src\trial_manager.c') -Raw

if ($Header -notmatch '#define\s+TRIAL_DURATION_TICKS\s+\(1500U\)') {
    throw '15 秒试验必须定义为 1500 个 10 ms tick。'
}
if ($Source -notmatch 'gStatus\.trialTicks\s*>=\s*TRIAL_DURATION_TICKS') {
    throw '试验完成条件必须使用统一的 TRIAL_DURATION_TICKS。'
}

Write-Host '15-second trial duration contract passed.'
