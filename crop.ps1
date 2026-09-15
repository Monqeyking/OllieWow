<#
.SYNOPSIS
  Snijdt een rechthoek uit een PNG en schaalt hem op, zodat tooltip- en
  UI-tekst leesbaar wordt in een screenshot van 3440x1351.

.EXAMPLE
  .\crop.ps1 -In hover.png -X 2612 -Y 670 -W 441 -H 176 -Scale 3 -Out tip.png
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory=$true)][string]$In,
  [Parameter(Mandatory=$true)][int]$X,
  [Parameter(Mandatory=$true)][int]$Y,
  [Parameter(Mandatory=$true)][int]$W,
  [Parameter(Mandatory=$true)][int]$H,
  [int]$Scale = 3,
  [Parameter(Mandatory=$true)][string]$Out
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName PresentationCore
Add-Type -AssemblyName WindowsBase

$src = New-Object System.Windows.Media.Imaging.BitmapImage
$src.BeginInit()
$src.UriSource = New-Object System.Uri((Resolve-Path $In).Path)
$src.CacheOption = [System.Windows.Media.Imaging.BitmapCacheOption]::OnLoad
$src.EndInit()

$px = [int][Math]::Max(0, [Math]::Min($X, $src.PixelWidth - 1))
$py = [int][Math]::Max(0, [Math]::Min($Y, $src.PixelHeight - 1))
$pw = [int][Math]::Min($W, $src.PixelWidth - $px)
$ph = [int][Math]::Min($H, $src.PixelHeight - $py)

$crop = New-Object System.Windows.Media.Imaging.CroppedBitmap($src, (New-Object System.Windows.Int32Rect($px, $py, $pw, $ph)))
$scaled = New-Object System.Windows.Media.Imaging.TransformedBitmap
$scaled.BeginInit()
$scaled.Source = $crop
$scaled.Transform = New-Object System.Windows.Media.ScaleTransform($Scale, $Scale)
$scaled.EndInit()

$encoder = New-Object System.Windows.Media.Imaging.PngBitmapEncoder
$encoder.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($scaled))
$stream = [System.IO.File]::Create($Out)
try { $encoder.Save($stream) } finally { $stream.Close() }
Write-Host ('Crop: ' + $Out + '  (' + $pw + 'x' + $ph + ' x' + $Scale + ')')
