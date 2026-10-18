param([string]$Tag = 'par', [int]$Clients = 4, [int]$Start = 0, [string]$List = '', [int]$Minutes = 60, [string]$Work = 'D:\MxM\showcase_data\runs')
$b = 'D:\MxM\GameServer\build'
$c = 'D:\MxM\Client\Client.Win32'
$py = 'D:\MxM\showcase_data\venv\Scripts\python.exe'
New-Item -ItemType Directory -Force $Work | Out-Null
Start-Process powershell -Verb RunAs -ArgumentList '-NoProfile', '-Command', 'Stop-Process -Name MXMClient -Force -ErrorAction SilentlyContinue' -Wait -ErrorAction SilentlyContinue
Set-Content -Path "$b\hub.cfg" -Value ("DevShowcase=1`nDevShowcaseStart=$Start" + $(if ($List) { "`nDevShowcaseList=$List" } else { "" })) -Encoding ascii -NoNewline
$gc = (Get-Content "$b\game.cfg") | Where-Object { $_ -notmatch '^DevShowcase|^DevMirror' }
$gc += 'DevShowcase=1', 'DevShowcaseCount=0', 'DevMirror=0'
$gc | Set-Content "$b\game.cfg" -Encoding ascii
$mmc = (Get-Content "$b\matchmaker.cfg") | Where-Object { $_ -notmatch '^SoloRooms' }
($mmc + 'SoloRooms=1') | Set-Content "$b\matchmaker.cfg" -Encoding ascii
Stop-Process -Name login_srv, hub_srv, game_srv, mm_srv -Force -ErrorAction SilentlyContinue
Start-Sleep 2
foreach ($s in 'mm_srv', 'hub_srv', 'game_srv', 'login_srv') { Start-Process "$b\$s.exe" -WorkingDirectory $b -WindowStyle Minimized; Start-Sleep 2 }
Start-Sleep 3
$pids = @()
for ($i = 0; $i -lt $Clients; $i++) {
    $login = if ($i -eq 0) { 'alexis' } else { "alexis$($i + 1)" }
    $p = Start-Process "$c\MXMClient.exe" -ArgumentList '/LogEncryption', '/AuthMethod:local', '/Network:dev', '/PacketEncryption:0', '/AutoJoinGame', "/AutoLoginID:$login" -WorkingDirectory $c -PassThru
    $pids += $p.Id
    Start-Sleep 8
}
Start-Sleep 15
$cap = "$Work\cap_$Tag"
$capProc = Start-Process $py -ArgumentList '-I', "`"$PSScriptRoot\wingrid.py`"", ($pids -join ','), "`"$cap`"", ($Minutes * 60), 0.15 -PassThru -WindowStyle Hidden
$t0 = Get-Date
while ($true) {
    $done = (Select-String -Path "$b\game_server.log" -Pattern 'Showcase :: done' -ErrorAction SilentlyContinue | Measure-Object).Count
    if ($done -ge $Clients) { $status = 'done'; break }
    if (-not (Get-Process game_srv -ErrorAction SilentlyContinue)) { $status = 'crashed'; break }
    if (((Get-Date) - $t0).TotalMinutes -gt $Minutes) { $status = 'timeout'; break }
    Start-Sleep 10
}
Copy-Item "$b\game_server.log" "$Work\$Tag.log"
Stop-Process -Id $capProc.Id -Force -ErrorAction SilentlyContinue
Start-Process powershell -Verb RunAs -ArgumentList '-NoProfile', '-Command', 'Stop-Process -Name MXMClient -Force' -Wait -ErrorAction SilentlyContinue
(Get-Content "$b\game.cfg") -replace '^DevShowcase=1', 'DevShowcase=0' | Set-Content "$b\game.cfg" -Encoding ascii
Set-Content -Path "$b\hub.cfg" -Value "DevShowcase=0`nDevShowcaseStart=0" -Encoding ascii -NoNewline
((Get-Content "$b\matchmaker.cfg") | Where-Object { $_ -notmatch '^SoloRooms' }) + 'SoloRooms=0' | Set-Content "$b\matchmaker.cfg" -Encoding ascii
Stop-Process -Name login_srv, hub_srv, game_srv, mm_srv -Force -ErrorAction SilentlyContinue
& $py -I "$PSScriptRoot\splitlog.py" "$Work\$Tag.log"
"parallel $Tag : $status ($done games done)"
