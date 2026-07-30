<# MPU6050 预置重校准 API 与当前禁用状态的静态契约。 #>
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Header = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'inc\mpu6050.h')
$Source = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\mpu6050.c')
$Main = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\main.c')

if ($Header -notmatch 'void\s+MPU6050_RecalibrateForNextRun\(void\);') {
    throw '缺少供未来启动流程调用的 MPU6050 重校准 API 声明。'
}
if ($Source -notmatch 'void\s+MPU6050_RecalibrateForNextRun\(void\)\s*\{\s*MPU6050_InitAndCalibrate\(\);\s*\}') {
    throw '重校准 API 必须复用既有 MPU6050 初始化与校准路径。'
}
if ($Main -match 'MPU6050_InitAndCalibrate\(\);') {
    throw '当前 PID 调参阶段不得在上电时校准 MPU6050。'
}
foreach ($RelativePath in @('src\main.c', 'src\line_run.c', 'src\UART_Auto_PID.c')) {
    $Text = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot $RelativePath)
    if ($Text -match 'MPU6050_RecalibrateForNextRun\(') {
        throw "当前 PID 调参阶段不得调用重校准 API: $RelativePath"
    }
}

Write-Host 'Deferred MPU recalibration contract passed.'
