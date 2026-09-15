<#
.SYNOPSIS
  Differentiele hover-probe. Doet de hover (bag-item -> eraf) meerdere keren
  achter elkaar op een enkele TCP-verbinding en samplert GameTooltip op ~25 Hz.
  Doel: onderscheiden of de terugspringende tooltip uit de async
  item-template-query komt (alleen de EERSTE hover, daarna cache) of niet.
#>
[CmdletBinding()]
param(
  [int]$BagSlotX = 3100,
  [int]$BagSlotY = 857,
  [int]$StepBack = 140,
  [int]$Cycles = 3,
  [int]$Samples = 18,
  [int]$SampleMs = 40
)
$ErrorActionPreference = 'Stop'

$endpointPath = @(
  'D:\OllieWoW\Client\artifacts\debug-control-endpoint.json',
  'D:\OllieWoW\Client\Logs\debug-control-endpoint.json'
) | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $endpointPath) { throw 'Geen debug-control endpoint gevonden.' }

$ep = Get-Content $endpointPath -Raw | ConvertFrom-Json
$client = New-Object System.Net.Sockets.TcpClient
$client.Connect([string]$ep.address, [int]$ep.port)
$stream = $client.GetStream()
$writer = New-Object System.IO.StreamWriter($stream, (New-Object System.Text.UTF8Encoding($false)))
$writer.NewLine = [char]10
$reader = New-Object System.IO.StreamReader($stream, [System.Text.Encoding]::UTF8)
$script:id = 0

function Send([string]$method, $params) {
  $script:id++
  $req = [ordered]@{ id = $script:id; capability = [string]$ep.capability_token; method = $method }
  if ($null -ne $params) { $req['params'] = $params }
  $writer.WriteLine(($req | ConvertTo-Json -Compress -Depth 8))
  $writer.Flush()
  $line = $reader.ReadLine()
  if ([string]::IsNullOrWhiteSpace($line)) { throw 'Lege response' }
  return ($line | ConvertFrom-Json)
}

function MoveCursor([int]$x, [int]$y) {
  $null = Send 'input.submit' ([ordered]@{
    type = 'mouse_motion'; timestamp_ms = 0; window_id = 0; device_id = 0
    button_mask = 0; x_pixels = $x; y_pixels = $y
    relative_x_pixels = 0; relative_y_pixels = 0 })
}

function Tip {
  $r = Send 'inspect.ui' ([ordered]@{
    selector = 'GameTooltip'; max_results = 1; include_lua = $false; include_ancestors = $false })
  return $r.result.nodes[0]
}

function Vis { return (Tip).effectivelyVisible }

for ($c = 1; $c -le $Cycles; $c++) {
  Write-Output ('=== ronde ' + $c + ' ===')
  MoveCursor $BagSlotX $BagSlotY
  Start-Sleep -Milliseconds 900
  Write-Output ('  op-item          vis=' + (Vis))

  MoveCursor ($BagSlotX - $StepBack) $BagSlotY
  $sw = [System.Diagnostics.Stopwatch]::StartNew()
  $first = $true
  $verdict = ''
  for ($i = 1; $i -le $Samples; $i++) {
    Start-Sleep -Milliseconds $SampleMs
    $v = Vis
    if ($first) { Write-Output ('  eraf t=' + $sw.ElapsedMilliseconds + 'ms   vis=' + $v); $first = $false; $prev = $v; continue }
    if ($v -ne $prev) {
      Write-Output ('  >>> WISSEL t=' + $sw.ElapsedMilliseconds + 'ms   vis=' + $v)
      $prev = $v
    }
  }
  Write-Output ('  einde t=' + $sw.ElapsedMilliseconds + 'ms   vis=' + $prev)

  MoveCursor 1700 400
  Start-Sleep -Milliseconds 400
}

$client.Close()
