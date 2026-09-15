<#
.SYNOPSIS
  Client voor het OpenWow debug-control kanaal (OPENWOW_DEBUG_CONTROL=1).

.DESCRIPTION
  Praat newline-gedelimitete JSON met de draaiende client via het endpoint in
  <diagnostic_output_root>\debug-control-endpoint.json.

  Protocol (debug_control_server.cpp / debug_control_json_codec.cpp):
    request : {"id":N,"capability":"<token>","method":"...","params":{...}}
    framing : één JSON-frame per regel, afgesloten met \n
    methoden: health | capabilities | inspect_frame | inspect_ui |
              capture_screenshot | submit_input

.EXAMPLE
  .\devctl.ps1 health
  .\devctl.ps1 reload
  .\devctl.ps1 text "/dump"
  .\devctl.ps1 key 40 13
  .\devctl.ps1 ui ActionButton1
  .\devctl.ps1 shot C:\temp\shot.png
#>
[CmdletBinding()]
param(
  [Parameter(Position = 0)][string]$Command = 'health',
  [Parameter(Position = 1, ValueFromRemainingArguments = $true)][string[]]$Rest
)

$ErrorActionPreference = 'Stop'

$candidates = @(
  'D:\OllieWoW\Client\artifacts\debug-control-endpoint.json',
  'D:\OllieWoW\Client\Logs\debug-control-endpoint.json'
)
$endpointPath = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $endpointPath) {
  Write-Host 'Geen debug-control endpoint gevonden. Start de client met OPENWOW_DEBUG_CONTROL=1' -ForegroundColor Yellow
  Write-Host ('Gezocht: ' + ($candidates -join ', '))
  exit 2
}

$endpoint = Get-Content $endpointPath -Raw | ConvertFrom-Json
$address  = if ($endpoint.address) { $endpoint.address } else { '127.0.0.1' }
$port     = [int]$endpoint.port
$token    = [string]$endpoint.capability_token

function Send-DebugRequest {
  param([string]$Method, $Params)

  $request = [ordered]@{ id = 1; capability = $token; method = $Method }
  if ($null -ne $Params) { $request['params'] = $Params }
  $json = ($request | ConvertTo-Json -Compress -Depth 8)

  $client = New-Object System.Net.Sockets.TcpClient
  try {
    $client.Connect($address, $port)
    $stream = $client.GetStream()
    $writer = New-Object System.IO.StreamWriter($stream, (New-Object System.Text.UTF8Encoding($false)))
    $writer.NewLine = "`n"
    $writer.WriteLine($json)
    $writer.Flush()

    $reader = New-Object System.IO.StreamReader($stream, [System.Text.Encoding]::UTF8)
    $line = $reader.ReadLine()
    if ([string]::IsNullOrWhiteSpace($line)) { throw 'Lege response van debug-control' }
    return ($line | ConvertFrom-Json)
  } finally {
    $client.Close()
  }
}

function New-TextEvent([string]$Text) {
  return [ordered]@{ type = 'text'; timestamp_ms = 0; window_id = 0; text = $Text }
}

function New-KeyEvent([int]$Scancode, [int]$Keycode, [bool]$Pressed = $true) {
  return [ordered]@{
    type = 'key'; timestamp_ms = 0; window_id = 0;
    scancode = $Scancode; keycode = $Keycode; modifiers = 0;
    pressed = $Pressed; repeat = $false
  }
}

# Klik-aliassen in SDL-nummering (glue_client.cpp zet het veld 1-op-1 in
# SDL_MouseButtonEvent::button): 1=links, 2=midden, 3=rechts.
if ($Command.ToLowerInvariant() -in @('lclick', 'rclick', 'mclick')) {
  $buttonNumber = @{ lclick = 1; mclick = 2; rclick = 3 }[$Command.ToLowerInvariant()]
  $Command = 'click'
  $Rest = @([string]$buttonNumber) + $Rest
}

function Send-Text([string]$Text) {
  # SDL_TextInputEvent::text is 32 bytes inclusief NUL. Grotere brokken worden
  # door de client geweigerd met "text input does not fit
  # SDL_TextInputEvent::text" (glue_client.cpp SubmitDebugInput). Hak de tekst
  # daarom in stukken van maximaal 31 UTF-8-bytes; de editbox plakt ze aan
  # elkaar.
  $chunk = ''
  $last = $null
  foreach ($ch in $Text.ToCharArray()) {
    $candidate = $chunk + $ch
    if ([System.Text.Encoding]::UTF8.GetByteCount($candidate) -gt 31) {
      if ($chunk.Length -gt 0) {
        $last = Send-DebugRequest 'input.submit' (New-TextEvent $chunk)
        Start-Sleep -Milliseconds 50
      }
      $chunk = [string]$ch
    } else {
      $chunk = $candidate
    }
  }
  if ($chunk.Length -gt 0) {
    $last = Send-DebugRequest 'input.submit' (New-TextEvent $chunk)
  }
  return $last
}

function Ensure-ChatFocus {
  # Enter is een toggle: staat het chatvenster al open, dan verzendt Enter de
  # huidige inhoud en sluit het venster. Lees daarom eerst de zichtbaarheid van
  # ChatFrameEditBox uit voordat we Enter sturen.
  $probe = Send-DebugRequest 'inspect.ui' ([ordered]@{
    selector = 'ChatFrameEditBox'; max_results = 1
    include_lua = $false; include_ancestors = $false })
  $open = $false
  if ($probe.ok -and $probe.result.matchedFrames -gt 0) {
    $open = [bool]$probe.result.nodes[0].locallyVisible
  }
  if (-not $open) {
    $null = Send-DebugRequest 'input.submit' (New-KeyEvent 40 13 $true)
    Start-Sleep -Milliseconds 250
  }
}

$result = switch ($Command.ToLowerInvariant()) {
  'health'  { Send-DebugRequest 'health' $null }
  'caps'    { Send-DebugRequest 'capabilities' $null }
  'ui'      {
    $selector = if ($Rest.Count -gt 0) { $Rest[0] } else { '' }
    Send-DebugRequest 'inspect.ui' ([ordered]@{ selector = $selector; max_results = 64; include_lua = $true; include_ancestors = $true })
  }
  'frame'   { Send-DebugRequest 'inspect.frame' $null }
  'text'    {
    if ($Rest.Count -eq 0) { throw 'Gebruik: devctl.ps1 text "<tekst>"' }
    Send-Text ($Rest -join ' ')
  }
  'key'     {
    if ($Rest.Count -lt 2) { throw 'Gebruik: devctl.ps1 key <scancode> <keycode> [down|up]' }
    $pressed = -not ($Rest.Count -ge 3 -and $Rest[2] -eq 'up')
    Send-DebugRequest 'input.submit' (New-KeyEvent ([int]$Rest[0]) ([int]$Rest[1]) $pressed)
  }
  'enter'   { Send-DebugRequest 'input.submit' (New-KeyEvent 40 13 $true) }
  'move'    {
    # Muispositie in drawable pixels (zoals de router ze gebruikt).
    if ($Rest.Count -lt 2) { throw 'Gebruik: devctl.ps1 move <x> <y>' }
    Send-DebugRequest 'input.submit' ([ordered]@{
      type = 'mouse_motion'; timestamp_ms = 0; window_id = 0; device_id = 0;
      button_mask = 0; x_pixels = [int]$Rest[0]; y_pixels = [int]$Rest[1];
      relative_x_pixels = 0; relative_y_pixels = 0 })
  }
  'click'   {
    # Klik op drawable-pixel (x,y): eerst muisbeweging, dan press+release.
    # Dezelfde coordinaten gaan mee in het button-event zodat de router ze
    # niet naar (0,0) laat vallen.
    if ($Rest.Count -lt 3) { throw 'Gebruik: devctl.ps1 click <1|2|3> <x> <y>' }
    $button = [int]$Rest[0]
    $cx = [int]$Rest[1]
    $cy = [int]$Rest[2]
    $null = Send-DebugRequest 'input.submit' ([ordered]@{
      type = 'mouse_motion'; timestamp_ms = 0; window_id = 0; device_id = 0;
      button_mask = 0; x_pixels = $cx; y_pixels = $cy;
      relative_x_pixels = 0; relative_y_pixels = 0 })
    Start-Sleep -Milliseconds 220
    $down = Send-DebugRequest 'input.submit' ([ordered]@{
      type = 'mouse_button'; timestamp_ms = 0; window_id = 0; device_id = 0;
      button = $button; pressed = $true; click_count = 1;
      x_pixels = $cx; y_pixels = $cy })
    Start-Sleep -Milliseconds 90
    $up = Send-DebugRequest 'input.submit' ([ordered]@{
      type = 'mouse_button'; timestamp_ms = 0; window_id = 0; device_id = 0;
      button = $button; pressed = $false; click_count = 1;
      x_pixels = $cx; y_pixels = $cy })
    [ordered]@{ button = $button; x = $cx; y = $cy; down_ok = $down.ok; up_ok = $up.ok
                down_error = $down.error; up_error = $up.error }
  }
  'lua'     {
    # Voert Lua uit in de draaiende client via het /run slash-commando
    # (slash_command_handler.cpp registreert script/run/dump).
    if ($Rest.Count -eq 0) { throw 'Gebruik: devctl.ps1 lua "<lua>"' }
    $code = '/run ' + ($Rest -join ' ')
    Write-Host $code
    # Het chatvenster moet focus hebben; een text-event zonder focus wordt
    # weggegooid.
    Ensure-ChatFocus
    $null = Send-Text $code
    Start-Sleep -Milliseconds 200
    Send-DebugRequest 'input.submit' (New-KeyEvent 40 13 $true)
  }
  'runfile' {
    # Voert Lua uit vanuit een bestand. Het bestand moet een enkele regel zijn
    # (het chatvenster kan geen newlines bevatten); newlines worden vervangen
    # door spaties. Dit omzeilt het quoting-probleem van geneste PowerShell.
    if ($Rest.Count -eq 0) { throw 'Gebruik: devctl.ps1 runfile <pad>' }
    $path = $Rest[0]
    if (-not (Test-Path $path)) { throw ('Niet gevonden: ' + $path) }
    $code = ((Get-Content $path -Raw) -replace '?
', ' ').Trim()
    while ($code.Contains('  ')) { $code = $code.Replace('  ', ' ') }
    Write-Host ('lua (' + $code.Length + ' tekens): ' + $code.Substring(0, [Math]::Min(80, $code.Length)) + '...')
    Ensure-ChatFocus
    $null = Send-Text ('/run ' + $code)
    Start-Sleep -Milliseconds 250
    Send-DebugRequest 'input.submit' (New-KeyEvent 40 13 $true)
  }
  'reload'  {
    # Tekst typen in het chatvenster + Enter. De client registreert /reload als
    # slash-commando (slash_command_handler.cpp) -> ReloadUI.
    Ensure-ChatFocus
    Write-Host 'text /reload'
    $null = Send-Text '/reload'
    Start-Sleep -Milliseconds 200
    Write-Host 'key ENTER'
    Send-DebugRequest 'input.submit' (New-KeyEvent 40 13 $true)
  }
  'shot'    {
    $response = Send-DebugRequest 'capture.screenshot' $null
    if ($response.ok -and $response.result) {
      $data = $response.result
      if ($data.path) { Write-Host ('Screenshot: ' + $data.path) -ForegroundColor Green }
      if ($data.bytes) {
        $out = if ($Rest.Count -gt 0) { $Rest[0] } else { Join-Path $PWD 'shot.png' }
        [System.IO.File]::WriteAllBytes($out, [Convert]::FromBase64String($data.bytes))
        Write-Host ('Screenshot: ' + $out) -ForegroundColor Green
      }
      if (-not $data.path -and -not $data.bytes) {
        Write-Host 'Onbekende screenshot-vorm; ruwe response volgt.' -ForegroundColor Yellow
        $response | ConvertTo-Json -Depth 6
      }
    } else {
      $response | ConvertTo-Json -Depth 6
    }
  }
  default { throw ('Onbekend commando: ' + $Command) }
}

if ($Command.ToLowerInvariant() -ne 'shot') {
  $result | ConvertTo-Json -Depth 8
}
