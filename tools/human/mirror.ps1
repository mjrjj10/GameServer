param([string]$List = '7,35', [int]$Pass = 0, [string]$Tag = 'm', [string]$Work = 'D:\MxM\showcase_data\runs', [int]$Minutes = 40, [int]$Steps = 0, [int]$Count = 2)
$b = 'D:\MxM\GameServer\build'
$c = 'D:\MxM\Client\Client.Win32'
$py = 'D:\MxM\showcase_data\venv\Scripts\python.exe'
Start-Process powershell -Verb RunAs -ArgumentList '-NoProfile', '-Command', 'Stop-Process -Name MXMClient -Force -ErrorAction SilentlyContinue; Get-Process python -ErrorAction SilentlyContinue | Where-Object { $_.Path -like ''*showcase_data*'' } | Stop-Process -Force' -Wait -ErrorAction SilentlyContinue
Set-Content -Path "$b\hub.cfg" -Value "DevShowcase=1`nDevShowcaseStart=0`nDevShowcaseList=$List`nDevMirror=1`nDevMirrorPass=$Pass" -Encoding ascii -NoNewline
$gc = (Get-Content "$b\game.cfg") | Where-Object { $_ -notmatch '^DevShowcase|^DevMirror' }
$gc += 'DevShowcase=1', "DevShowcaseCount=$Count", 'DevMirror=1', "DevShowcaseSteps=$Steps"
$gc | Set-Content "$b\game.cfg" -Encoding ascii
Stop-Process -Name login_srv, hub_srv, game_srv, mm_srv -Force -ErrorAction SilentlyContinue
Start-Sleep 2
foreach ($s in 'mm_srv', 'hub_srv', 'game_srv', 'login_srv') { Start-Process "$b\$s.exe" -WorkingDirectory $b -WindowStyle Minimized; Start-Sleep 2 }
Start-Sleep 3
$cl = Start-Process "$c\MXMClient.exe" -ArgumentList '/LogEncryption', '/AuthMethod:local', '/Network:dev', '/PacketEncryption:0', '/AutoJoinGame', '/AutoLoginID:alexis' -WorkingDirectory $c -PassThru
Start-Sleep 20
$clientPid = (Get-Process MXMClient | Sort-Object StartTime -Descending)[0].Id
$agentLog = "$Work\$Tag`_agent.log"
Start-Process $py -Verb RunAs -WindowStyle Hidden -ArgumentList '-I', "`"$PSScriptRoot\mirror_agent.py`"", $clientPid, "`"$b\game_server.log`"", "`"$agentLog`"", ($Minutes * 60)
$cap = "$Work\cap_$Tag"
$job = Start-Job -ScriptBlock { param($f, $o, $s) powershell -ExecutionPolicy Bypass -File $f -OutDir $o -Seconds $s -IntervalMs 400 } -ArgumentList "D:\MxM\GameServer\tools\showcase\capture.ps1", $cap, ($Minutes * 60)
$t0 = Get-Date
while ($true) {
    if (Select-String -Path "$b\game_server.log" -Pattern 'Showcase :: done' -Quiet -ErrorAction SilentlyContinue) { $status = 'done'; break }
    if (-not (Get-Process game_srv -ErrorAction SilentlyContinue)) { $status = 'crashed'; break }
    if (((Get-Date) - $t0).TotalMinutes -gt $Minutes) { $status = 'timeout'; break }
    Start-Sleep 10
}
Copy-Item "$b\game_server.log" "$Work\$Tag.log"
Stop-Job $job -ErrorAction SilentlyContinue; Remove-Job $job -Force -ErrorAction SilentlyContinue
Start-Process powershell -Verb RunAs -ArgumentList '-NoProfile', '-Command', 'Stop-Process -Name MXMClient -Force; Get-Process python -ErrorAction SilentlyContinue | Where-Object { $_.Path -like ''*showcase_data*'' } | Stop-Process -Force' -Wait -ErrorAction SilentlyContinue
(Get-Content "$b\game.cfg") -replace '^DevShowcase=1', 'DevShowcase=0' -replace '^DevMirror=1', 'DevMirror=0' | Set-Content "$b\game.cfg" -Encoding ascii
Set-Content -Path "$b\hub.cfg" -Value "DevShowcase=0`nDevShowcaseStart=0" -Encoding ascii -NoNewline
Stop-Process -Name login_srv, hub_srv, game_srv, mm_srv -Force -ErrorAction SilentlyContinue
"mirror $Tag : $status"
