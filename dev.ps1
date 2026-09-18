<#
.SYNOPSIS
  Incrementele dev-build voor OpenWow: compileert alleen wat geraakt is en linkt
  de client. Vervangt de volledige 'cmake --build', die in deze boom blijft
  hangen op de CMake-herconfiguratiestap (VERIFY_GLOBS -> RERUN_CMAKE).

.DESCRIPTION
  Bepaalt de gewijzigde bronbestanden via git, zoekt met ninja's depfiles op
  welke objecten daarvan afhangen, compileert die met de MSVC-omgeving en linkt
  de betrokken libs plus OllieWoW.exe.

.PARAMETER Run
  Start de client na een geslaagde build (Start-OllieWoW-GossipDebug.cmd).

.PARAMETER NoLink
  Alleen compileren, niet linken.

.EXAMPLE
  .\dev.ps1
  .\dev.ps1 -Run
#>
[CmdletBinding()]
param(
  [switch]$Run,
  [switch]$NoLink,
  [switch]$All
)

$ErrorActionPreference = 'Stop'
$repo    = 'D:\OllieWoW\Experiments\OpenWow-snapshot'
$build   = Join-Path $repo 'build\release'
$ninja   = Join-Path $repo '.vcpkg\downloads\tools\ninja-1.13.2-windows\ninja.exe'
$vcvars  = 'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat'
$clientExe = Join-Path $build 'apps\client\OllieWoW.exe'

$started = Get-Date

if (-not (Test-Path $ninja))  { throw "ninja niet gevonden: $ninja" }
if (-not (Test-Path $build))  { throw "build-map niet gevonden: $build" }

# --- 1. Gewijzigde bronbestanden bepalen -------------------------------------
$changed = @()
if ($All) {
  Write-Host 'Alle bronbestanden worden opnieuw gecompileerd (-All).' -ForegroundColor Yellow
} else {
  Push-Location $repo
  try {
    $changed = & git status --porcelain 2>$null |
      ForEach-Object { $_.Substring(3).Trim() } |
      Where-Object { $_ -match '\.(cpp|c|h|hpp|inc)$' } |
      ForEach-Object { Join-Path $repo ($_ -replace '/', '\') } |
      Where-Object { Test-Path $_ }
  } finally { Pop-Location }
}

if (-not $All -and $changed.Count -eq 0) {
  Write-Host 'Geen gewijzigde bronbestanden. Niets te doen.' -ForegroundColor Green
  exit 0
}

Write-Host ("Gewijzigd: " + $changed.Count + " bestand(en)") -ForegroundColor Cyan
$changed | ForEach-Object { Write-Host ('  ' + $_.Substring($repo.Length + 1)) }

# Nieuwste wijzigingstijd van de gewijzigde bronnen. Objecten die daarna al
# gebouwd zijn, hoeven niet opnieuw -- zie stap 2.
$newestSourceUtc = $null
foreach ($src in $changed) {
  if (Test-Path -LiteralPath $src) {
    $t = (Get-Item -LiteralPath $src).LastWriteTimeUtc
    if ($null -eq $newestSourceUtc -or $t -gt $newestSourceUtc) { $newestSourceUtc = $t }
  }
}

# --- 2. Geraakte objecten bepalen via ninja's depfiles -----------------------
Push-Location $build
try {
  # Alle targets één keer ophalen: gebruikt voor de dep-analyse en om te
  # controleren dat een afgeleide lib ook echt een lib-target is (apps/client
  # heet bijvoorbeeld CMakeFiles/openwow-client.dir maar levert een .exe).
  $allTargets = & $ninja -t targets all 2>$null | ForEach-Object { ($_ -split ':')[0] }
  $knownLibTargets = New-Object System.Collections.Generic.HashSet[string]
  foreach ($t in $allTargets) { if ($t -match '\.lib$') { [void]$knownLibTargets.Add($t) } }

  $objs = New-Object System.Collections.Generic.HashSet[string]

  if ($All) {
    $allTargets | Where-Object { $_ -match '\.obj$' } | ForEach-Object { [void]$objs.Add($_) }
  } else {
    $needles = $changed | ForEach-Object { ($_ -split '[\\/]')[-1] }
    $deps = & $ninja -t deps 2>$null
    $current = $null
    foreach ($line in $deps) {
      if ($line -match '^(.+?):\s+#deps') { $current = $Matches[1]; continue }
      if (-not $current) { continue }
      $trimmed = $line.Trim()
      if ([string]::IsNullOrWhiteSpace($trimmed)) { continue }
      $leaf = ($trimmed -split '[\\/]')[-1]
      if ($needles -contains $leaf) { [void]$objs.Add($current) }
    }
    # Een gewijzigde .cpp is geen eigen dependency: het object zelf toevoegen.
    foreach ($src in $changed) {
      if ($src -notmatch '\.(cpp|c)$') { continue }
      $leaf = (($src -split '[\\/]')[-1]) + '.obj'
      $allTargets | Where-Object { $_ -match ('\.dir[\\/]') -and $_ -like "*$leaf" } |
        ForEach-Object { [void]$objs.Add($_) }
    }
  }

  $objs = @($objs | Where-Object { $_ -match '\.obj$' } | Sort-Object -Unique)
  if ($objs.Count -eq 0) {
    Write-Host 'Geen geraakte objecten gevonden. Niets te doen.' -ForegroundColor Green
    exit 0
  }

  # Sla objecten over die al nieuwer zijn dan elk gewijzigd bronbestand. Zonder
  # deze filter herbouwt elke run alles wat van een gewijzigde header afhangt:
  # cgobject.h raakt ~400 objecten, dus vijf minuten voor een wijziging van één
  # regel. De LIBS leiden we uit de volledige set af, zodat een eerdere run die
  # na het compileren afbrak (bijvoorbeeld omdat de client de exe vasthield)
  # alsnog linkt zonder alles opnieuw te compileren.
  $compileObjs = $objs
  if ($null -ne $newestSourceUtc -and -not $All) {
    $compileObjs = @($objs | Where-Object {
      $objPath = Join-Path $build $_
      if (-not (Test-Path -LiteralPath $objPath)) { return $true }
      return (Get-Item -LiteralPath $objPath).LastWriteTimeUtc -lt $newestSourceUtc
    })
    if ($compileObjs.Count -lt $objs.Count) {
      Write-Host ("Al actueel, overgeslagen: " + ($objs.Count - $compileObjs.Count) + " object(en)") -ForegroundColor DarkGray
    }
  }
  Write-Host ("Objecten: " + $compileObjs.Count + " te compileren van " + $objs.Count) -ForegroundColor Cyan

  # --- 3. Compileer- en linkcommando's ophalen -------------------------------
  # De objecten gaan als argumenten mee naar 'ninja -t commands'. Raakt een
  # breed geincludeerde header (cgobject.h zit in ~400 objecten), dan liep die
  # argumentenlijst tegen de Windows-limiet aan: "Program 'ninja.exe' failed to
  # run: De bestandsnaam of -extensie is te lang". Daarom in brokken.
  $cmds = @()
  if ($compileObjs.Count -gt 0) {
    $batchSize = 40
    for ($offset = 0; $offset -lt $compileObjs.Count; $offset += $batchSize) {
      $last = [Math]::Min($offset + $batchSize - 1, $compileObjs.Count - 1)
      $cmds += & $ninja -t commands @($compileObjs[$offset..$last]) 2>$null
    }
  }
  $compile = @($cmds | Where-Object { $_ -match 'cl\.exe' -and $_ -match '\s-c\s' } | Sort-Object -Unique)
  if ($compileObjs.Count -gt 0 -and $compile.Count -eq 0) { throw "Geen compileercommando's gevonden." }

  $libs = New-Object System.Collections.Generic.HashSet[string]
  foreach ($o in $objs) {
    if ($o -match '^(.*)/CMakeFiles/([^/]+)\.dir/') {
      $candidate = $Matches[1] + '/' + $Matches[2] + '.lib'
      if ($knownLibTargets.Contains($candidate)) { [void]$libs.Add($candidate) }
    }
  }

  $links = @()
  if (-not $NoLink) {
    $linkTargets = @($libs) + @('openwow-client')
    $lcmds = & $ninja -t commands @linkTargets 2>$null
    $want = New-Object System.Collections.Generic.HashSet[string]
    foreach ($l in $libs) { [void]$want.Add((($l -split '[\\/]')[-1])) }
    [void]$want.Add('OllieWoW.exe')
    $seen = New-Object System.Collections.Generic.HashSet[string]
    $ordered = New-Object System.Collections.Generic.List[string]
    foreach ($c in $lcmds) {
      if ($c -notmatch '(?i)/out:') { continue }
      $m = [regex]::Match($c, '(?i)/out:(\S+)')
      if (-not $m.Success) { continue }
      if (-not $want.Contains((($m.Groups[1].Value -split '[\\/]')[-1]))) { continue }
      if ($seen.Add($c)) { $ordered.Add($c) }
    }
    $links = @($ordered)
  }
  Write-Host ("Compiles: " + $compile.Count + "   Links: " + $links.Count) -ForegroundColor Cyan

  # --- 4. Uitvoeren via .bat-bestanden met de MSVC-omgeving ----------------
  # De compilatie gaat in N parallelle bats. Sequentieel kostte een brede
  # header (cgobject.h raakt ~400 objecten) bijna een uur; met N workers is
  # dat een paar minuten. De LINKS blijven sequentieel -- die schrijven
  # gedeelde uitvoerbestanden.
  $logDir = $build
  $workers = [Math]::Min(16, [Math]::Max(1, [int]([Environment]::ProcessorCount / 2)))
  if ($compile.Count -lt 8) { $workers = 1 }

  $chunks = @()
  for ($w = 0; $w -lt $workers; $w++) { $chunks += ,(New-Object System.Collections.Generic.List[string]) }
  $idx = 0
  foreach ($c in $compile) { $chunks[$idx % $workers].Add($c); $idx++ }

  $procs = @()
  for ($w = 0; $w -lt $workers; $w++) {
    if ($chunks[$w].Count -eq 0) { continue }
    $logFile = Join-Path $logDir ('dev-build-' + $w + '.log')
    $batLines = New-Object System.Collections.Generic.List[string]
    $batLines.Add('@echo off')
    $batLines.Add('call "' + $vcvars + '" >nul 2>&1')
    $i = 0
    foreach ($c in $chunks[$w]) {
      $i++
      $batLines.Add('echo compile ' + $i + '/' + $chunks[$w].Count)
      $batLines.Add($c + ' > "' + $logFile + '" 2>&1')
      $batLines.Add('if errorlevel 1 (echo COMPILE_FAILED & powershell -NoProfile -Command "Get-Content ''' + $logFile + ''' -Tail 30" & exit /b 1)')
      # Een CMake-wrapper kan ondanks een fout toch 0 teruggeven; kijk de log na.
      $batLines.Add('findstr /I /C:"fatal error" /C:"error LNK" /C:"error C2" /C:"error C3" "' + $logFile + '" >nul 2>&1')
      $batLines.Add('if not errorlevel 1 (echo COMPILE_LOG_ERROR & powershell -NoProfile -Command "Get-Content ''' + $logFile + ''' -Tail 30" & exit /b 1)')
    }
    # Sentinelbestand in plaats van de exitcode van het Process-object:
    # Start-Process -PassThru met -RedirectStandardOutput laat ExitCode null, en
    # $null -ne 0 is waar -- dan geldt elk geslaagd werk als mislukt. De bat
    # schrijft dit bestand alleen als alle compiles van deze worker slaagden.
    $sentinel = Join-Path $build ('dev-ok-' + $w + '.txt')
    if (Test-Path $sentinel) { Remove-Item $sentinel -Force }
    $batLines.Add('echo ok> "' + $sentinel + '"')
    # Zonder expliciete exit erft de bat het errorlevel van de laatste
    # findstr-controle, en die geeft 1 terug als hij niets vindt -- het goede geval.
    $batLines.Add('exit /b 0')
    $batPath = Join-Path $build ('dev-compile-' + $w + '.bat')
    $batLines | Set-Content -Path $batPath -Encoding ASCII
    $outFile = Join-Path $logDir ('dev-compile-' + $w + '.out')
    $procs += Start-Process -FilePath 'cmd.exe' -ArgumentList '/D','/S','/C', $batPath -NoNewWindow -PassThru -RedirectStandardOutput $outFile -RedirectStandardError ($outFile + '.err')
  }

  Write-Host ('Compileren met ' + $procs.Count + ' workers...') -ForegroundColor Cyan
  foreach ($p in $procs) { $p.WaitForExit() }
  $failed = @()
  for ($w = 0; $w -lt $workers; $w++) {
    if ($chunks[$w].Count -eq 0) { continue }
    if (-not (Test-Path (Join-Path $build ('dev-ok-' + $w + '.txt')))) { $failed += $w }
  }
  if ($failed.Count -gt 0) {
    foreach ($w in 0..($workers - 1)) {
      $o = Join-Path $logDir ('dev-compile-' + $w + '.out')
      if (Test-Path $o) { Write-Host ('--- worker ' + $w + ' ---') -ForegroundColor Yellow; Get-Content $o -Tail 20 }
    }
    throw 'Compilatie mislukt.'
  }

  # --- 5. Linken (sequentieel) ---------------------------------------------
  $linkLog = Join-Path $logDir 'dev-link.log'
  $linkLines = New-Object System.Collections.Generic.List[string]
  $linkLines.Add('@echo off')
  $linkLines.Add('call "' + $vcvars + '" >nul 2>&1')
  foreach ($c in $links) {
    $out = ([regex]::Match($c, '(?i)/out:(\S+)')).Groups[1].Value
    $linkLines.Add('echo link ' + $out)
    $linkLines.Add($c + ' > "' + $linkLog + '" 2>&1')
    $linkLines.Add('if errorlevel 1 (echo LINK_FAILED ' + $out + ' & powershell -NoProfile -Command "Get-Content ''' + $linkLog + ''' -Tail 30" & exit /b 1)')
    $linkLines.Add('findstr /I /C:"fatal error" /C:"error LNK" "' + $linkLog + '" >nul 2>&1')
    $linkLines.Add('if not errorlevel 1 (echo LINK_LOG_ERROR ' + $out + ' & powershell -NoProfile -Command "Get-Content ''' + $linkLog + ''' -Tail 30" & exit /b 1)')
  }
  $linkLines.Add('echo DEV_BUILD_OK')
  $linkPath = Join-Path $build 'dev-link.bat'
  $linkLines | Set-Content -Path $linkPath -Encoding ASCII

  & cmd.exe /D /S /C $linkPath
  if ($LASTEXITCODE -ne 0) { throw ('Linken mislukt (exit ' + $LASTEXITCODE + '). Zie ' + $linkLog) }
} finally { Pop-Location }

$elapsed = [int]((Get-Date) - $started).TotalSeconds
$exeTime = if (Test-Path $clientExe) { (Get-Item $clientExe).LastWriteTime.ToString('HH:mm:ss') } else { '?' }
Write-Host ''
Write-Host ('Klaar in ' + $elapsed + 's. OllieWoW.exe: ' + $exeTime) -ForegroundColor Green

if ($Run) {
  $launcher = Join-Path $build 'apps\client\Start-OllieWoW-Dev.cmd'
  if (Test-Path $launcher) {
    Write-Host 'Client starten...' -ForegroundColor Cyan
    Start-Process -FilePath $launcher -WorkingDirectory (Split-Path $launcher)
  } else {
    Write-Host ('Launcher niet gevonden: ' + $launcher) -ForegroundColor Yellow
  }
}
