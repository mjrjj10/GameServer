param([string]$Script, [string]$Log)
$py = 'D:\MxM\showcase_data\venv\Scripts\python.exe'
$drv = Join-Path $PSScriptRoot 'drive.py'
Start-Process $py -Verb RunAs -WindowStyle Hidden -Wait -ArgumentList '-I', "`"$drv`"", "`"$Script`"", "`"$Log`""
if (Test-Path $Log) { Get-Content $Log -Tail 30 }
