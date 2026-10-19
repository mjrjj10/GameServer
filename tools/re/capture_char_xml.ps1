param([string]$List = '1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,43', [int]$Minutes = 320)
$frida = 'D:\MxM\re_tools\frida-inject.exe'
$agent = "$PSScriptRoot\agent_dumpchar.js"
$log = 'D:\MxM\re_tools\capture_char_xml.log'
$job = Start-Process powershell -ArgumentList '-ExecutionPolicy', 'Bypass', '-File', "$PSScriptRoot\..\showcase\runpar.ps1", '-Tag', 'capxml', '-Clients', '1', '-List', $List, '-Minutes', $Minutes -PassThru -WindowStyle Minimized
$attached = @{}
while (-not $job.HasExited) {
    foreach ($p in Get-Process MXMClient -ErrorAction SilentlyContinue) {
        if ($attached.ContainsKey($p.Id)) { continue }
        Start-Sleep 20
        Start-Process $frida -ArgumentList '-p', $p.Id, '-s', $agent -RedirectStandardOutput $log -WindowStyle Hidden | Out-Null
        $attached[$p.Id] = 1
        "attached $($p.Id)"
    }
    Start-Sleep 5
}
"capture finished; run: python -I D:\MxM\GameServer\tools\kf\export_movegraph.py"
