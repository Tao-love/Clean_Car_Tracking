$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$encoderPath = Join-Path $projectRoot 'src\encoder.c'
$motorPath = Join-Path $projectRoot 'src\motor.c'
$encoder = Get-Content -Raw -LiteralPath $encoderPath
$motor = Get-Content -Raw -LiteralPath $motorPath

if ($encoder -notmatch '#define\s+ENCODER_RIGHT_REVERSE\s*\(\s*1\s*\)') {
    throw '右轮正 PWM 对应车辆前进时，右编码器必须反相，使前进速度保持为正。'
}

if ($encoder -notmatch 'static void Encoder_HandleRightEdge\(void\)\s*\{[\s\S]*?if\s*\(\s*ENCODER_RIGHT_REVERSE\s*\)\s*\{\s*step\s*=\s*-step;\s*\}') {
    throw '右轮边沿处理必须把 ENCODER_RIGHT_REVERSE 应用于解码后的步进。'
}

if ($motor -notmatch 'static void Motor_SetRightDirection\(bool forward\)\s*\{[\s\S]*?if\s*\(forward\)\s*\{[\s\S]*?DL_GPIO_setPins\(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_BIN1_PIN\);[\s\S]*?DL_GPIO_clearPins\(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_BIN2_PIN\);[\s\S]*?\}\s*else\s*\{[\s\S]*?DL_GPIO_clearPins\(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_BIN1_PIN\);[\s\S]*?DL_GPIO_setPins\(GPIO_MOTOR_PORT, GPIO_MOTOR_MOTOR_BIN2_PIN\);') {
    throw '右轮正 PWM 必须使用 BIN1 高、BIN2 低，才能使右轮朝车辆前方转。'
}

Write-Host 'Right motor direction and encoder sign contract passed.'
