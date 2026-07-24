<#
手动调参启动路径契约。
不访问硬件；确认烧录版本不会读取旧 Flash 参数，KEY1 会启动循线，
正常完成后回到 IDLE，而故障仍保留原状态。
#>

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$MainSource = Get-Content -LiteralPath (Join-Path $ProjectRoot 'src\main.c') -Raw
$TrialSource = Get-Content -LiteralPath (Join-Path $ProjectRoot 'src\trial_manager.c') -Raw

function Require-Match([string]$Text, [string]$Pattern, [string]$Message) {
    if ($Text -notmatch $Pattern) {
        throw $Message
    }
}

function Require-NoMatch([string]$Text, [string]$Pattern, [string]$Message) {
    if ($Text -match $Pattern) {
        throw $Message
    }
}

Require-NoMatch $MainSource 'FlashParams_LoadLatest|#include "flash_params\.h"' `
    '手动调参启动路径不得加载历史 Flash 参数。'
Require-Match $MainSource 'TrialManager_Init\(0U,\s*0,\s*0U\);' `
    'main 必须以空 Flash 参数初始化 TrialManager。'
Require-Match $MainSource 'TrialManager_Start\(tick,\s*TRIAL_MODE_LINE_FOLLOW,\s*0,\s*0,\s*TRIAL_START_SOURCE_KEY1\)' `
    'KEY1 必须启动零命令的 TRIAL_MODE_LINE_FOLLOW。'
Require-Match $TrialSource 'gStatus\.state = \(reason == STOP_REASON_TRIAL_COMPLETE\) \?\s*SYSTEM_STATE_IDLE : state;' `
    '只有正常完成的试验应回到 IDLE；故障必须保留 FAULT。'

Write-Host 'Manual tuning source contract passed.'
