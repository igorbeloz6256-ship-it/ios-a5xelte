# Builds a50alsa (static Linux arm64, no libc) with the Arm GNU aarch64-none-elf toolchain.
#   powershell -File tools\a50-alsa\build.ps1 [-Gcc PATH] [-Out PATH]
param(
    [string]$Gcc = "E:\iOS-A50\tools\arm-gnu\bin\aarch64-none-elf-gcc.exe",
    [string]$Out = "$PSScriptRoot\a50alsa"
)
$ErrorActionPreference = 'Stop'
& $Gcc -O2 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -nostdlib -static -Wall -Wextra `
    '-Wl,-e,_start' '-Wl,--build-id=none' '-Wl,-Ttext=0x400000' -o $Out "$PSScriptRoot\a50alsa.c"
if ($LASTEXITCODE -ne 0) { throw "build failed" }
Write-Host "built $Out ($((Get-Item $Out).Length) bytes)"
