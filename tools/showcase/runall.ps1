param([string]$Tag = 'run', [int[]]$Starts = @(0, 10, 20, 30), [string]$List = '', [int]$Count = 0, [string]$Work = 'D:\MxM\showcase_data\runs')
$b = 'D:\MxM\GameServer\build'
New-Item -ItemType Directory -Force $Work | Out-Null
foreach ($start in $Starts) {
    Start-Process powershell -Verb RunAs -ArgumentList '-NoProfile', '-Command', 'Stop-Process -Name MXMClient -Force' -Wait -ErrorAction SilentlyContinue
    Set-Content -Path "$b\hub.cfg" -Value ("DevShowcase=1`nDevShowcaseStart=$start" + $(if ($List) { "`nDevShowcaseList=$List" } else { "" })) -Encoding ascii -NoNewline
    $gc = (Get-Content "$b\game.cfg") | Where-Object { $_ -notmatch '^DevShowcase' }
    $gc += "DevShowcase=1"
    $gc += "DevShowcaseCount=$Count"
    $gc | Set-Content "$b\game.cfg" -Encoding ascii
    Stop-Process -Name login_srv, hub_srv, game_srv, mm_srv -Force -ErrorAction SilentlyContinue
    Start-Sleep 2
    foreach ($s in 'mm_srv', 'hub_srv', 'game_srv', 'login_srv') { Start-Process "$b\$s.exe" -WorkingDirectory $b -WindowStyle Minimized; Start-Sleep 2 }
    Start-Sleep 3
    Start-Process 'D:\MxM\Client\Client.Win32\MXMClient.exe' -ArgumentList '/LogEncryption', '/AuthMethod:local', '/Network:dev', '/PacketEncryption:0', '/AutoJoinGame', '/AutoLoginID:alexis' -WorkingDirectory 'D:\MxM\Client\Client.Win32'
    $t0 = Get-Date
    while (-not (Select-String -Path "$b\game_server.log" -Pattern 'Showcase :: placement' -Quiet -ErrorAction SilentlyContinue)) {
        if (((Get-Date) - $t0).TotalMinutes -gt 10) { break }
        if (-not (Get-Process game_srv -ErrorAction SilentlyContinue)) { Start-Sleep 8; if (-not (Get-Process game_srv -ErrorAction SilentlyContinue)) { break } }
        Start-Sleep 2
    }
    $cap = "$Work\cap_$Tag`_$start"
    $job = Start-Job -ScriptBlock { param($f, $o) powershell -ExecutionPolicy Bypass -File $f -OutDir $o -Seconds 3000 -IntervalMs 400 } -ArgumentList "$PSScriptRoot\capture.ps1", $cap
    $t0 = Get-Date
    while ($true) {
        if (Select-String -Path "$b\game_server.log" -Pattern 'Showcase :: done' -Quiet -ErrorAction SilentlyContinue) { $status = 'done'; break }
        if (-not (Get-Process game_srv -ErrorAction SilentlyContinue)) { $status = 'crashed'; break }
        if (((Get-Date) - $t0).TotalMinutes -gt 50) { $status = 'timeout'; break }
        Start-Sleep 10
    }
    Copy-Item "$b\game_server.log" "$Work\$Tag`_$start.log"
    Stop-Job $job -ErrorAction SilentlyContinue; Remove-Job $job -Force -ErrorAction SilentlyContinue
    "batch $start : $status"
}
Start-Process powershell -Verb RunAs -ArgumentList '-NoProfile', '-Command', 'Stop-Process -Name MXMClient -Force' -Wait -ErrorAction SilentlyContinue
(Get-Content "$b\game.cfg") -replace '^DevShowcase=1', 'DevShowcase=0' | Set-Content "$b\game.cfg" -Encoding ascii
Set-Content -Path "$b\hub.cfg" -Value "DevShowcase=0`nDevShowcaseStart=0" -Encoding ascii -NoNewline
Stop-Process -Name login_srv, hub_srv, game_srv, mm_srv -Force -ErrorAction SilentlyContinue
