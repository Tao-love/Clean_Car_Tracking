<# 50 轮 Clean_Car speed 阶段自动调参的六项 Q16 输出契约。 #>
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Source = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'src\control_params.c')

foreach ($pattern in @(
    '#define\s+CLEAN_CAR_EXPORTED_LEFT_KP_Q16\s+\(17694720L\)',
    '#define\s+CLEAN_CAR_EXPORTED_LEFT_KI_Q16\s+\(81920L\)',
    '#define\s+CLEAN_CAR_EXPORTED_RIGHT_KP_Q16\s+\(25559040L\)',
    '#define\s+CLEAN_CAR_EXPORTED_RIGHT_KI_Q16\s+\(42598L\)',
    '#define\s+CLEAN_CAR_EXPORTED_LINE_P_Q16\s+\(936L\)',
    '#define\s+CLEAN_CAR_EXPORTED_LINE_D_Q16\s+\(204L\)',
    'CLEAN_CAR_EXPORTED_LEFT_KP_Q16,\s*CLEAN_CAR_EXPORTED_LEFT_KI_Q16,\s*7L \* Q16_ONE,',
    'CLEAN_CAR_EXPORTED_RIGHT_KP_Q16,\s*CLEAN_CAR_EXPORTED_RIGHT_KI_Q16,\s*6L \* Q16_ONE,',
    'CLEAN_CAR_EXPORTED_LINE_P_Q16,\s*CLEAN_CAR_EXPORTED_LINE_D_Q16,'
)) {
    if ($Source -notmatch $pattern) {
        throw "自动调参 Q16 输出未写入默认控制参数: $pattern"
    }
}

Write-Host 'Auto-tuned six-gain control-parameter contract passed.'
