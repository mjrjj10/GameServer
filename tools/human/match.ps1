param([string]$PickA = '1377 367|524 367', [string]$PickB = '1377 544|1257 367', [string]$Work = 'D:\MxM\showcase_data\human')
$c = 'D:\MxM\Client\Client.Win32'
$args0 = '/LogEncryption', '/AuthMethod:local', '/Network:dev', '/PacketEncryption:0', '/AutoJoinGame'
New-Item -ItemType Directory -Force $Work | Out-Null
Start-Process powershell -Verb RunAs -ArgumentList '-NoProfile', '-Command', 'Stop-Process -Name MXMClient -Force' -Wait -ErrorAction SilentlyContinue
$a = Start-Process "$c\MXMClient.exe" -ArgumentList ($args0 + '/AutoLoginID:alexis') -WorkingDirectory $c -PassThru
Start-Sleep 8
$b = Start-Process "$c\MXMClient.exe" -ArgumentList ($args0 + '/AutoLoginID:test2') -WorkingDirectory $c -PassThru
Start-Sleep 25
$pa = (Get-Process MXMClient | Sort-Object StartTime)[0].Id
$pb = (Get-Process MXMClient | Sort-Object StartTime)[1].Id
function Steps($p) { $p.Split('|') | ForEach-Object { "click $_"; 'sleep 0.6' } }
$lines = @()
foreach ($p in $pa, $pb) { $lines += "focus $p", 'sleep 1', 'click 960 70', 'sleep 2.5', 'click 649 300', 'sleep 2.5', 'click 431 357', 'sleep 2.5' }
foreach ($p in $pa, $pb) { $lines += "focus $p", 'sleep 1', 'click 960 1007', 'sleep 2' }
foreach ($p in $pa, $pb) { $lines += "focus $p", 'sleep 1', 'click 960 1007', 'sleep 1.5' }
$lines += 'sleep 14'
foreach ($p in $pa, $pb) { $lines += "focus $p", 'sleep 0.7', 'click 888 651', 'sleep 0.7' }
$lines += 'sleep 4', "focus $pa", 'sleep 0.7'
$lines += Steps $PickA
$lines += 'click 1056 847', 'sleep 1.2', 'click 960 1007', 'sleep 0.8', "focus $pb", 'sleep 0.8'
$lines += Steps $PickB
$lines += 'click 1056 847', 'sleep 1.2', 'click 960 1007', 'sleep 1', "shot $Work\picked.jpg"
$script = Join-Path $Work 'match_steps.txt'
$lines | Set-Content $script -Encoding ascii
& (Join-Path $PSScriptRoot 'drive.ps1') -Script $script -Log (Join-Path $Work 'drive.log') | Out-Null
"alexis=$pa test2=$pb"
