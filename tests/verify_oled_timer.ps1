<# OLED 计时器复位与首帧显示契约。 #>
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Source = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\Oled_Timer.c')

if ($Source -notmatch 'DL_TimerG_stopCounter\(TIMG6\)') {
    throw '停止计时必须停止 TIMG6，避免下一轮从旧计数相位继续。'
}
if ($Source -notmatch 'DL_TimerG_setTimerCount\(TIMG6,\s*32767U\)') {
    throw '启动/停止计时必须把 TIMG6 计数器复位为完整 1 秒周期。'
}
if ($Source -notmatch 'DL_TimerG_clearInterruptStatus\(TIMG6,\s*DL_TIMERG_INTERRUPT_ZERO_EVENT\)') {
    throw '复位计时器时必须清除遗留 ZERO 中断状态。'
}
if ($Source -notmatch 'gOledLastDisplayed\s*=\s*UINT32_MAX') {
    throw '每轮启动必须强制刷新首帧 00，而不能沿用上一轮显示值。'
}

Write-Host 'OLED timer reset and first-frame contract passed.'
