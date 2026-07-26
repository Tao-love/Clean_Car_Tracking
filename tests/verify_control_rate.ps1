<#
150 Hz 控制周期契约。
#>

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Syscfg = Get-Content -LiteralPath (Join-Path $ProjectRoot 'empty.syscfg') -Raw

if ($Syscfg -notmatch 'TIMER_CONTROL\.timerPeriod\s*=\s*"6\.667 ms";') {
    throw '控制周期必须为 6.667 ms。'
}
Write-Host '150 Hz control-rate contract passed.'
