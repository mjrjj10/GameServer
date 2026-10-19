param([string]$Tag = 'reg', [string]$Base = 'D:\MxM\GameServer_base\build', [string]$New = 'D:\MxM\GameServer\build', [int]$Minutes = 90)
$work = 'D:\MxM\showcase_data\runs'
$list = (1..40) -join ','
powershell -ExecutionPolicy Bypass -File "$PSScriptRoot\runpar.ps1" -Tag "$Tag`_base" -List $list -Minutes $Minutes -Build $Base
powershell -ExecutionPolicy Bypass -File "$PSScriptRoot\runpar.ps1" -Tag "$Tag`_new" -List $list -Minutes $Minutes -Build $New
python -I "$PSScriptRoot\regress_compare.py" "$work\$Tag`_base.log" "$work\$Tag`_new.log" "$work\$Tag`_report.md"
"night regression $Tag finished"
