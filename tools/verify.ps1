# ----- AI
<#
basic-1 完整离线验证。

职责：重生成 SysConfig，运行 Python 测试，使用 TI Arm Clang 严格编译/链接所有固件源码，
并验证 Flash 双槽的绝对地址。脚本不烧录、不打开 COM 口、不启动电机。
#>

param(
    [string]$SdkRoot = 'C:\ti\ti\mspm0_sdk_2_11_00_07',
    [string]$CompilerRoot = 'C:\ti\ti_cgt_arm_llvm_4.0.2.LTS',
    [string]$SysConfigCli = 'C:\ti\ccs\utils\sysconfig_1.28.0\sysconfig_cli.bat',
    [string]$PythonExe = 'E:\python\python.exe'
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Output = Join-Path $ProjectRoot '.build\verify'
New-Item -ItemType Directory -Force $Output | Out-Null

foreach ($required in @(
    (Join-Path $SdkRoot '.metadata\product.json'),
    (Join-Path $CompilerRoot 'bin\tiarmclang.exe'),
    $SysConfigCli,
    $PythonExe
)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "缺少验证依赖: $required"
    }
}

Push-Location $ProjectRoot
try {
    & $SysConfigCli `
        --product (Join-Path $SdkRoot '.metadata\product.json') `
        --script '.\empty.syscfg' `
        --output '.\syscfg_gen' `
        --compiler ticlang `
        --treatWarningsAsErrors
    if ($LASTEXITCODE -ne 0) { throw 'SysConfig 生成失败' }

    $env:PYTHONPATH = 'python;python\tests'
    & $PythonExe -m unittest discover -s python\tests -v
    if ($LASTEXITCODE -ne 0) { throw 'Python 测试失败' }
    & $PythonExe -m compileall -q python\autotune
    if ($LASTEXITCODE -ne 0) { throw 'Python 语法检查失败' }

    $Compiler = Join-Path $CompilerRoot 'bin\tiarmclang.exe'
    $Common = @(
        '@syscfg_gen/device.opt',
        '-march=thumbv6m', '-mcpu=cortex-m0plus', '-mfloat-abi=soft',
        '-mlittle-endian', '-mthumb', '-O2',
        ('-I' + (Join-Path $SdkRoot 'source\third_party\CMSIS\Core\Include')),
        ('-I' + (Join-Path $SdkRoot 'source')),
        '-Isyscfg_gen', '-Iinc', '-gdwarf-3',
        '-Wall', '-Wextra', '-Werror'
    )
    $Sources = @('.\syscfg_gen\ti_msp_dl_config.c') +
        @(Get-ChildItem '.\src\*.c' -File | ForEach-Object { $_.FullName })
    $Objects = @()
    foreach ($Source in $Sources) {
        $Object = Join-Path $Output (([IO.Path]::GetFileNameWithoutExtension($Source)) + '.o')
        & $Compiler @Common -c $Source -o $Object
        if ($LASTEXITCODE -ne 0) { throw "固件编译失败: $Source" }
        $Objects += $Object
    }

    $Map = Join-Path $Output 'basic-1.map'
    $Firmware = Join-Path $Output 'basic-1.out'
    $DriverLib = Join-Path $SdkRoot 'source\ti\driverlib\lib\ticlang\m0p\mspm0g1x0x_g3x0x\driverlib.a'
    $LinkArgs = @(
        '@syscfg_gen/device.opt',
        '-march=thumbv6m', '-mcpu=cortex-m0plus', '-mfloat-abi=soft',
        '-mlittle-endian', '-mthumb', '-O2', '-gdwarf-3',
        '-Wall', '-Wextra', '-Werror', '-Wl,--rom_model',
        ('-Wl,-m' + $Map), '-o', $Firmware
    ) + $Objects + @('-Wl,syscfg_gen\device_linker.cmd', $DriverLib)
    & $Compiler @LinkArgs
    if ($LASTEXITCODE -ne 0) { throw '固件链接失败' }

    $MapText = Get-Content -Raw -LiteralPath $Map
    if (($MapText -notmatch '0001f800.+gFlashParamsSlotA') -or
        ($MapText -notmatch '0001fc00.+gFlashParamsSlotB')) {
        throw 'Flash 双槽未固定在 0x1F800/0x1FC00'
    }
    Write-Host "验证通过: $Firmware"
}
finally {
    Pop-Location
}
# ----- AI
