# Write an image to the A50 BOOT partition from TWRP over adb, with checks.
# Only BOOT is ever written. Does not reboot the phone.
#
#   powershell -File tools\a50-adb-boot.ps1 -Image E:\iOS-A50\flash\probe1\boot.img
#   powershell -File tools\a50-adb-boot.ps1 -Image E:\iOS-A50\backup\2026-11-13\boot.img   # restore stock
param(
    [Parameter(Mandatory = $true)][string]$Image,
    [string]$Adb = "E:\iOS-A50\tools\platform-tools\adb.exe"
)
$ErrorActionPreference = 'Stop'
$Part = '/dev/block/by-name/boot'
$Tmp = '/tmp/a50-boot.img'

function Fail($msg) { Write-Host "FAIL: $msg"; exit 1 }
function PhoneSha($cmd) { ((& $Adb shell $cmd) -split '\s+')[0].Trim().ToLower() }

$item = Get-Item -LiteralPath $Image
$size = $item.Length
$sha = (Get-FileHash -Algorithm SHA256 -LiteralPath $Image).Hash.ToLower()
$head = [byte[]]::new(8)
$fs = [IO.File]::OpenRead($item.FullName); [void]$fs.Read($head, 0, 8); $fs.Close()
if ([Text.Encoding]::ASCII.GetString($head) -ne 'ANDROID!') { Fail "$Image is not an Android BOOT image" }

$state = (& $Adb get-state 2>$null)
if ($state -ne 'recovery') { Fail "phone is not in TWRP over adb (state: '$state')" }
$partSize = [int64](& $Adb shell blockdev --getsize64 $Part)
if ($size -gt $partSize) { Fail "image $size bytes > BOOT $partSize bytes" }
Write-Host "image  $($item.FullName)  $size bytes  sha256 $sha"
Write-Host "BOOT   $partSize bytes"

& $Adb push $item.FullName $Tmp | Out-Null
if ((PhoneSha "sha256sum $Tmp") -ne $sha) { Fail "pushed copy differs; BOOT untouched" }

& $Adb shell "dd if=$Tmp of=$Part bs=4096 2>/dev/null && sync" | Out-Null
$back = PhoneSha "head -c $size $Part | sha256sum"
& $Adb shell rm -f $Tmp | Out-Null
if ($back -ne $sha) { Fail "read-back of BOOT differs ($back) - restore the stock boot before rebooting" }
Write-Host "OK: BOOT now starts with this image (read-back sha256 matches). Not rebooted."
