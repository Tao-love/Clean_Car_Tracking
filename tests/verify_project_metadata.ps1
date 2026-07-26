<# Clean_Car 工程身份与唯一 XDS110 调试入口契约。 #>
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Project = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot '.project')
$CProject = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot '.cproject')
$CcsProject = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot '.ccsproject')
$Launch = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot '.theia\launch.json')
$Verify = Get-Content -Raw -LiteralPath (Join-Path $ProjectRoot 'tools\verify.ps1')

if ($Project -notmatch '<name>Clean_Car</name>') {
    throw '.project 工程名必须为 Clean_Car。'
}
if ($CProject -notmatch '<project id="Clean_Car\.com\.ti\.ccstudio') {
    throw '.cproject 内部工程 ID 必须以 Clean_Car 开头。'
}
if ($CcsProject -notmatch 'projectspec\.basic-1') {
    throw '.ccsproject 必须保留 TI projectspec.basic-1 来源模板标识。'
}
if (($Launch -notmatch '"name": "Clean_Car"') -or
    ($Launch -notmatch '"resourceId": "/Clean_Car"') -or
    ($Launch -notmatch 'Texas Instruments XDS110 USB Debug Probe') -or
    ($Launch -match 'UARTConnection|1_USB|MPU_Hand_Car')) {
    throw 'Theia 必须只包含 Clean_Car 的 XDS110 启动配置。'
}
if (($Verify -notmatch "'Clean_Car\.map'") -or
    ($Verify -notmatch "'Clean_Car\.out'") -or
    ($Verify -match "'MPU_Hand_Car\.(map|out)'")) {
    throw '严格构建输出必须统一为 Clean_Car.map/Clean_Car.out。'
}
foreach ($obsolete in @(
    'targetConfigs - 副本', 'targetConfigs - 副本 - 副本',
    'tools\export_usb_hex.ps1', 'legacy\teammate_initial',
    'tests\1.theia-workspace', 'work_filed'
)) {
    if (Test-Path -LiteralPath (Join-Path $ProjectRoot $obsolete)) {
        throw "旧工程入口或历史目录仍存在: $obsolete"
    }
}

Write-Host 'Clean_Car metadata and XDS110-only contract passed.'
