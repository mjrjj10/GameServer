param([string]$Mod = 'D:\MxM\ClientMod\Data', [string]$Client = 'D:\MxM\Client\Data', [switch]$Uninstall)
$manifest = Join-Path $Client 'cnport_installed.txt'
if ($Uninstall) {
    if (Test-Path $manifest) {
        Get-Content $manifest | ForEach-Object { Remove-Item -LiteralPath (Join-Path $Client $_) -Force -ErrorAction SilentlyContinue }
        Remove-Item $manifest
    }
    'uninstalled'
    return
}
$installed = @()
if (Test-Path $manifest) { $installed = Get-Content $manifest }
$added = 0; $updated = 0; $skipped = @()
Get-ChildItem -LiteralPath $Mod -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($Mod.Length + 1)
    $dst = Join-Path $Client $rel
    if ((Test-Path -LiteralPath $dst) -and ($installed -notcontains $rel)) { $skipped += $rel; return }
    New-Item -ItemType Directory -Force (Split-Path $dst) | Out-Null
    if ($installed -contains $rel) { $updated++ } else { $installed += $rel; $added++ }
    Copy-Item -LiteralPath $_.FullName -Destination $dst -Force
}
$installed | Set-Content $manifest
"added $added updated $updated skipped $($skipped.Count)"
$skipped | Select-Object -First 10
