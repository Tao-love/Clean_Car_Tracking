<# 10 秒单一循线和 KEY1 两步操作契约。 #>
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Header = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'inc\line_run.h')
$Source = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\line_run.c')
$Main = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\main.c')

if ($Header -notmatch '#define\s+LINE_RUN_DURATION_TICKS\s+\(1500U\)') {
    throw '10 秒运行必须定义为 1500 个 6.667 ms tick。'
}
foreach ($api in @(
    'bool LineRun_Init(void);', 'bool LineRun_Start(void);',
    'void LineRun_Stop(void);', 'void LineRun_ControlTick(void);',
    'bool LineRun_IsRunning(void);'
)) {
    if (-not $Header.Contains($api)) {
        throw "line_run API 缺失: $api"
    }
}
if ($Source -notmatch 'gRunTicks\+\+;' -or
    $Source -notmatch 'gRunTicks\s*>=\s*LINE_RUN_DURATION_TICKS') {
    throw '运行结束必须统一使用 1500 tick 上限。'
}
if ($Source -notmatch 'line\.error\s*=\s*gHasLastLineError\s*\?\s*gLastLineError\s*:\s*0;') {
    throw '无效灰度必须沿用本轮最后有效误差；尚无样本时使用 0。'
}
if ($Source -notmatch '\bMPU6050_Update\(\);') {
    throw '循线控制周期必须更新 MPU6050 角速度。'
}
if ($Source -notmatch 'LineControl_Step\(\s*&gLineState,\s*&line,\s*gParams,\s*MPU6050_GetTurnDamping\(\)\)') {
    throw '循线器必须接收 MPU6050 的转向阻尼。'
}
if ($Source -notmatch 'LineControl_Reset' -or
    $Source -notmatch 'SpeedPI_Reset' -or
    $Source -notmatch 'gLastLineError\s*=\s*0;' -or
    $Source -notmatch 'gRunTicks\s*=\s*0U;') {
    throw '停车/重启必须清除计时、PI、PD 和最后灰度误差。'
}
if ($Main -notmatch 'if\s*\(LineRun_IsRunning\(\)\)\s*\{\s*LineRun_Stop\(\);\s*\}\s*else\s*\{\s*\(void\) LineRun_Start\(\);') {
    throw 'KEY1 必须在运行中停车、空闲时启动，且不能同一次按键自动重启。'
}
if ($Source -match 'FAULT|SafetyGuard|Trial|Telemetry|Protocol|FlashParams') {
    throw 'line_run 不得残留试验、通信、遥测或故障锁存状态机。'
}

Write-Host 'Line-run duration, hold-last-error and KEY1 contract passed.'
