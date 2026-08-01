<# 固定实车参数和范围检查契约。 #>
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Header = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'inc\control_params.h')
$Source = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\control_params.c')

if ($Header -notmatch 'const\s+ControlParams\s+\*ControlParams_Get\(void\);') {
    throw '参数模块必须只读公开 ControlParams_Get()。'
}
if ($Header -notmatch 'const\s+ControlModeProfile\s+\*ControlParams_GetProfile\(ControlProfileId id\);') {
    throw '参数模块必须公开按 profile 读取接口。'
}
foreach ($pattern in @(
    '12L \* Q16_ONE, 0, 42L \* Q16_ONE,',
    '12L \* Q16_ONE, 0, \(69L \* Q16_ONE / 2\),',
    '\(Q16_ONE / 70\), \(Q16_ONE / 320\),',
    '\(5L \* Q16_ONE / 8\), 10000,',
    '15, 67, 35,',
    '600, 2333',
    'gProfiles\s*\[\s*CONTROL_PROFILE_COUNT\s*\]',
    '1200',
    '8000'
)) {
    if ($Source -notmatch $pattern) {
        throw "固定实车参数不匹配: $pattern"
    }
}
if ($Source -notmatch '#define\s+PARAM_PWM_MAX\s+\(1000\)' -or
    $Source -notmatch 'params->maxPwm\s*>\s*PARAM_PWM_MAX') {
    throw '参数校验必须将 maxPwm 硬限制为不超过 1000。'
}
if ($Source -match 'Stage|Pending|Wire|Version|Flash|Telemetry|Stall|Overrun') {
    throw '固定参数模块不得残留通信、Flash 或保护生命周期。'
}

Write-Host 'Fixed control-parameter contract passed.'
