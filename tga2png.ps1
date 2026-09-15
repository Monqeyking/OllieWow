<#
.SYNOPSIS
  Converteert een RLE-TGA screenshot van de client naar PNG zodat je hem kunt bekijken.

.DESCRIPTION
  De debug-control capture.screenshot schrijft TGA (imagetype 10, RLE, 24-bit BGR).
  Decoderen gebeurt in C# via Add-Type -- een PowerShell-pixel-lus over miljoenen
  pixels is te traag.

.EXAMPLE
  .\tga2png.ps1 -In shot.tga -Out shot.png
  .\tga2png.ps1            # nieuwste screenshot in Client\Screenshots
#>
[CmdletBinding()]
param(
  [string]$In,
  [string]$Out
)

$ErrorActionPreference = 'Stop'

if (-not $In) {
  $latest = Get-ChildItem 'D:\OllieWoW\Client\Screenshots' -Filter '*.tga' -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1
  if (-not $latest) { throw 'Geen TGA gevonden in Client\Screenshots' }
  $In = $latest.FullName
}
if (-not $Out) {
  $Out = [IO.Path]::ChangeExtension($In, '.png')
}

Add-Type -AssemblyName PresentationCore

if (-not ('TgaCodec' -as [type])) {
Add-Type -TypeDefinition @"
using System;
using System.IO;

public static class TgaCodec
{
    // Leest een uncompressed (2) of RLE (10) truecolor TGA en geeft BGRA-pixels terug.
    public static byte[] Decode(string path, out int width, out int height)
    {
        byte[] data = File.ReadAllBytes(path);
        int idLength = data[0];
        int imageType = data[2];
        width = data[12] | (data[13] << 8);
        height = data[14] | (data[15] << 8);
        int bpp = data[16];
        int bytesPerPixel = bpp / 8;
        int pos = 18 + idLength;
        int pixelCount = width * height;
        byte[] pixels = new byte[pixelCount * 4];

        if (imageType == 2)
        {
            for (int i = 0; i < pixelCount; i++)
            {
                pixels[i * 4] = data[pos];
                pixels[i * 4 + 1] = data[pos + 1];
                pixels[i * 4 + 2] = data[pos + 2];
                pixels[i * 4 + 3] = 255;
                pos += bytesPerPixel;
            }
        }
        else if (imageType == 10)
        {
            int written = 0;
            while (written < pixelCount)
            {
                int header = data[pos++];
                int count = (header & 0x7F) + 1;
                if ((header & 0x80) != 0)
                {
                    byte b = data[pos], g = data[pos + 1], r = data[pos + 2];
                    pos += bytesPerPixel;
                    for (int i = 0; i < count && written < pixelCount; i++)
                    {
                        pixels[written * 4] = b;
                        pixels[written * 4 + 1] = g;
                        pixels[written * 4 + 2] = r;
                        pixels[written * 4 + 3] = 255;
                        written++;
                    }
                }
                else
                {
                    for (int i = 0; i < count && written < pixelCount; i++)
                    {
                        pixels[written * 4] = data[pos];
                        pixels[written * 4 + 1] = data[pos + 1];
                        pixels[written * 4 + 2] = data[pos + 2];
                        pixels[written * 4 + 3] = 255;
                        pos += bytesPerPixel;
                        written++;
                    }
                }
            }
        }
        else
        {
            throw new NotSupportedException("TGA image type " + imageType + " wordt niet ondersteund");
        }
        return pixels;
    }
}
"@
}

$w = 0; $h = 0
$pixels = [TgaCodec]::Decode($In, [ref]$w, [ref]$h)
$bmp = [System.Windows.Media.Imaging.BitmapSource]::Create(
    $w, $h, 96, 96, [System.Windows.Media.PixelFormats]::Bgra32, $null, $pixels, $w * 4)
$encoder = New-Object System.Windows.Media.Imaging.PngBitmapEncoder
$encoder.Frames.Add([System.Windows.Media.Imaging.BitmapFrame]::Create($bmp))
$stream = [IO.File]::Create($Out)
try { $encoder.Save($stream) } finally { $stream.Close() }

Write-Output ('PNG: ' + $Out + '  (' + $w + 'x' + $h + ')')
