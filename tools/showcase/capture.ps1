param([string]$OutDir, [int]$Seconds = 600, [int]$IntervalMs = 500)
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
New-Item -ItemType Directory -Force $OutDir | Out-Null
$b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$end = (Get-Date).AddSeconds($Seconds)
while((Get-Date) -lt $end) {
  if(-not (Get-Process MXMClient -ErrorAction SilentlyContinue)) { break }
  $ms = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
  $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
  $small = New-Object System.Drawing.Bitmap $bmp, 1440, 810
  $small.Save((Join-Path $OutDir "$ms.jpg"), [System.Drawing.Imaging.ImageFormat]::Jpeg)
  $g.Dispose(); $bmp.Dispose(); $small.Dispose()
  Start-Sleep -Milliseconds $IntervalMs
}
