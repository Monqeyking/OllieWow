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
  Write-Host ("Objecten: " + $objs.Count) -ForegroundColor Cyan

  # --- 3. Compileer- en linkcommando's ophalen -------------------------------
  $cmds = & $ninja -t commands @objs 2>$null
  $compile = @($cmds | Where-Object { $_ -match 'cl\.exe' -and $_ -match '\s-c\s' } | Sort-Object -Unique)
  if ($compile.Count -eq 0) { throw "Geen compileercommando's gevonden." }

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

  # --- 4. Uitvoeren via een .bat met de MSVC-omgeving ------------------------
  $batLines = New-Object System.Collections.Generic.List[string]
  $batLines.Add('@echo off')
  $batLines.Add('call "' + $vcvars + '" >nul 2>&1')
  $logFile = Join-Path $build 'dev-build.log'
  $i = 0
  foreach ($c in $compile) {
    $i++
    $batLines.Add('echo compile ' + $i + '/' + $compile.Count)
    $batLines.Add($c + ' > "' + $logFile + '" 2>&1')
    $batLines.Add('if errorlevel 1 (echo COMPILE_FAILED_' + $i + ' & powershell -NoProfile -Command "Get-Content ''' + $logFile + ''' -Tail 30" & exit /b 1)')
    # De link/compile kan via een CMake-wrapper lopen die ondanks een fout toch 0
    # teruggeeft; daarom ook de log zelf nakijken op foutmarkers.
    $batLines.Add('findstr /I /C:"fatal error" /C:"error LNK" /C:"error C2" /C:"error C3" "' + $logFile + '" >nul 2>&1')
    $batLines.Add('if not errorlevel 1 (echo COMPILE_LOG_ERROR_' + $i + ' & powershell -NoProfile -Command "Get-Content ''' + $logFile + ''' -Tail 30" & exit /b 1)')
  }
  foreach ($c in $links) {
    $out = ([regex]::Match($c, '(?i)/out:(\S+)')).Groups[1].Value
    $batLines.Add('echo link ' + $out)
    $batLines.Add($c + ' > "' + $logFile + '" 2>&1')
    $batLines.Add('if errorlevel 1 (echo LINK_FAILED ' + $out + ' & powershell -NoProfile -Command "Get-Content ''' + $logFile + ''' -Tail 30" & exit /b 1)')
    $batLines.Add('findstr /I /C:"fatal error" /C:"error LNK" "' + $logFile + '" >nul 2>&1')
    $batLines.Add('if not errorlevel 1 (echo LINK_LOG_ERROR ' + $out + ' & powershell -NoProfile -Command "Get-Content ''' + $logFile + ''' -Tail 30" & exit /b 1)')
  }
  $batLines.Add('echo DEV_BUILD_OK')
  $batPath = Join-Path $build 'dev-build.bat'
  $batLines | Set-Content -Path $batPath -Encoding ASCII

  & cmd.exe /D /S /C $batPath
  if ($LASTEXITCODE -ne 0) { throw ('Build mislukt (exit ' + $LASTEXITCODE + '). Zie ' + $logFile) }
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
