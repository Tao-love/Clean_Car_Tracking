<#
150 Hz 控制周期与时间型安全保护契约。
#>

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Syscfg = Get-Content -LiteralPath (Join-Path $ProjectRoot 'empty.syscfg') -Raw
$Safety = Get-Content -LiteralPath (Join-Path $ProjectRoot 'inc\safety_guard.h') -Raw
$SafetySource = Get-Content -LiteralPath (Join-Path $ProjectRoot 'src\safety_guard.c') -Raw

if ($Syscfg -notmatch 'TIMER_CONTROL\.timerPeriod\s*=\s*"6\.667 ms";') {
    throw '控制周期必须为 6.667 ms。'
}
if ($Safety -notmatch '#define\s+SAFETY_LINE_LOST_TICKS\s+\(23U\)') {
    throw '丢线保护必须保持约 150 ms。'
}
if ($Safety -notmatch '#define\s+SAFETY_STALL_TICKS\s+\(45U\)') {
    throw '堵转保护必须保持约 300 ms。'
}
if ($SafetySource -match 'COMM_TIMEOUT|lastValidFrameTick|OnValidFrame|requireCommHeartbeat') {
    throw '通信超时安全路径必须彻底移除。'
}
Write-Host '150 Hz control-rate contract passed.'
