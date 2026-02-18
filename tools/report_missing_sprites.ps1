$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$charPath = Join-Path $root "data\characters.json"
$enemyPath = Join-Path $root "data\enemies.json"
$assets = Join-Path $root "data\assets"

function Load-Json($path) {
  Get-Content -Raw -Path $path | ConvertFrom-Json
}

function File-Exists($relPath) {
  Test-Path -Path (Join-Path $assets $relPath)
}

$chars = (Load-Json $charPath).characters
$enemies = (Load-Json $enemyPath).enemies

$missingChars = @()
foreach ($c in $chars) {
  $missing = @()
  $portrait = $c.portrait
  if ($portrait) {
    if (-not (File-Exists ("portraits\" + $portrait))) {
      $missing += ("portraits\" + $portrait)
    }
  } else {
    $missing += "portrait (missing field)"
  }
  $walk = $c.walk_strip
  if ($walk) {
    if (-not (File-Exists $walk)) {
      $missing += $walk
    }
  }
  if ($missing.Count -gt 0) {
    $missingChars += [PSCustomObject]@{ id = $c.id; missing = $missing }
  }
}

$baseEnemy = "enemies\goo_enemy.png"
$baseExists = File-Exists $baseEnemy
$special = @{
  eye = "enemies\eye_enemy.png"
  ghost = "enemies\ghost_enemy.png"
  charger = "enemies\reaper_enemy.png"
}

$missingEnemies = @()
foreach ($e in $enemies) {
  $missing = @()
  $id = $e.id
  if ($special.ContainsKey($id)) {
    $path = $special[$id]
    if (-not (File-Exists $path)) { $missing += $path }
  } else {
    if (-not $baseExists) { $missing += $baseEnemy }
  }
  if ($missing.Count -gt 0) {
    $missingEnemies += [PSCustomObject]@{ id = $id; missing = $missing }
  }
}

Write-Host "Missing hero sprites:"
if ($missingChars.Count -eq 0) {
  Write-Host "  (none)"
} else {
  foreach ($m in $missingChars) {
    Write-Host "  $($m.id):"
    foreach ($p in $m.missing) {
      Write-Host "    - $p"
    }
  }
}

Write-Host ""
Write-Host "Missing enemy sprites:"
if ($missingEnemies.Count -eq 0) {
  Write-Host "  (none)"
} else {
  foreach ($m in $missingEnemies) {
    Write-Host "  $($m.id):"
    foreach ($p in $m.missing) {
      Write-Host "    - $p"
    }
  }
}
