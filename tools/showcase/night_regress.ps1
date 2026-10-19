param([string]$Tag = 'reg', [string]$Base = 'D:\MxM\GameServer_base\build', [string]$New = 'D:\MxM\GameServer\build', [int]$Minutes = 35, [int]$Chunk = 10)
$work = 'D:\MxM\showcase_data\runs'
if (-not (Test-Path "$Base\game_srv.exe")) { "base build missing: $Base"; exit 1 }
$chunks = @()
for ($i = 1; $i -le 40; $i += $Chunk) { $chunks += ((($i)..([Math]::Min($i + $Chunk - 1, 40))) -join ',') }
foreach ($side in @(@('base', $Base), @('new', $New))) {
    $name = "$Tag`_$($side[0])"
    $parts = @()
    for ($k = 0; $k -lt $chunks.Count; $k++) {
        powershell -ExecutionPolicy Bypass -File "$PSScriptRoot\runpar.ps1" -Tag "$name`_c$k" -Clients 1 -List $chunks[$k] -Minutes $Minutes -Build $side[1]
        $parts += "$work\$name`_c$k.log"
    }
    Get-Content $parts | Set-Content "$work\$name.log" -Encoding utf8
}
python -I "$PSScriptRoot\regress_compare.py" "$work\$Tag`_base.log" "$work\$Tag`_new.log" "$work\$Tag`_report.md"
"night regression $Tag finished"
