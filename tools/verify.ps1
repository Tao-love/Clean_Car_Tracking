# ----- AI
<#
MSPM0 固件完整离线验证。

职责：重生成 SysConfig，使用 TI Arm Clang 严格编译/链接所有固件源码，
并验证 Flash 双槽的绝对地址。脚本不烧录、不打开 COM 口、不启动电机。
#>

param(
    [string]$SdkRoot = 'C:\ti\ti\mspm0_sdk_2_11_00_07',
    [string]$CompilerRoot = 'C:\ti\ti_cgt_arm_llvm_4.0.2.LTS',
    [string]$SysConfigCli = 'C:\ti\ccs\utils\sysconfig_1.28.0\sysconfig_cli.bat'
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Output = Join-Path $ProjectRoot '.build\verify'
New-Item -ItemType Directory -Force $Output | Out-Null

foreach ($required in @(
    (Join-Path $SdkRoot '.metadata\product.json'),
    (Join-Path $CompilerRoot 'bin\tiarmclang.exe'),
    $SysConfigCli
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

    # 纯 C 契约测试当前不能在本机执行 ARM 指令，但必须参加同等级严格交叉编译，避免测试源码失效。
    $ContractTestSource = '.\tests\firmware_host\test_core.c'
    $ContractTestObject = Join-Path $Output 'test_core.o'
    & $Compiler @Common -c $ContractTestSource -o $ContractTestObject
    if ($LASTEXITCODE -ne 0) { throw '固件纯 C 契约测试编译失败' }

    # 把契约测试与对应纯逻辑模块链接为独立 ARM ELF，确保接口和运行库符号完整；该 ELF 不用于烧录。
    $ContractFirmware = Join-Path $Output 'test_core.out'
    $ContractObjects = @(
        $ContractTestObject,
        (Join-Path $Output 'crc16.o'),
        (Join-Path $Output 'ring_buffer.o'),
        (Join-Path $Output 'uart_echo.o'),
        (Join-Path $Output 'manual_tuning.o'),
        (Join-Path $Output 'control_params.o'),
        (Join-Path $Output 'speed_pi.o'),
        (Join-Path $Output 'line_control.o'),
        (Join-Path $Output 'key_start.o'),
        (Join-Path $Output 'safety_guard.o'),
        (Join-Path $Output 'trial_stats.o')
    )
    $ContractLinkArgs = @(
        '@syscfg_gen/device.opt',
        '-march=thumbv6m', '-mcpu=cortex-m0plus', '-mfloat-abi=soft',
        '-mlittle-endian', '-mthumb', '-O2', '-gdwarf-3',
        '-Wall', '-Wextra', '-Werror', '-Wl,--rom_model',
        '-o', $ContractFirmware
    ) + $ContractObjects + @('-Wl,syscfg_gen\device_linker.cmd')
    & $Compiler @ContractLinkArgs
    if ($LASTEXITCODE -ne 0) { throw '固件纯 C 契约测试链接失败' }

    $Map = Join-Path $Output 'USB_nobluetooth.map'
    $Firmware = Join-Path $Output 'USB_nobluetooth.out'
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
