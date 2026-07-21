# ----- AI
# 将 CCS 构建得到的 MSPM0 ELF 固件转换为 Intel HEX，供 UART BSL 备用下载器使用。
param(
    [string]$InputElf = (Join-Path $PSScriptRoot '..\Debug\1_USB.out'),
    [string]$OutputHex = (Join-Path $PSScriptRoot '..\Debug\1_USB.hex'),
    [string]$Objcopy = 'C:\Ti\ti_cgt_arm_llvm_4.0.2.LTS\bin\tiarmobjcopy.exe'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $InputElf -PathType Leaf)) {
    throw "未找到已构建的固件：$InputElf；请先在 CCS 执行 Build Project。"
}

if (-not (Test-Path -LiteralPath $Objcopy -PathType Leaf)) {
    throw "未找到 TI objcopy：$Objcopy"
}

& $Objcopy -O ihex $InputElf $OutputHex
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $OutputHex -PathType Leaf)) {
    throw 'Intel HEX 导出失败。'
}

Write-Host "已生成：$OutputHex"
# ----- AI
