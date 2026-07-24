$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $projectRoot 'src\line_control.c'
$source = Get-Content -Raw -LiteralPath $sourcePath

if ($source -notmatch 'result\.leftTarget\s*=\s*LineControl_ClampTarget\(\s*\(int32_t\)\s*params->baseSpeed\s*\+\s*delta') {
    throw '正循线误差必须提高左轮目标速度，使车向右修正。'
}
if ($source -notmatch 'result\.rightTarget\s*=\s*LineControl_ClampTarget\(\s*\(int32_t\)\s*params->baseSpeed\s*-\s*delta') {
    throw '正循线误差必须降低右轮目标速度，使车向右修正。'
}

Write-Host 'Line turn direction contract passed.'
