param(
    [string]$DshHome = $(if ($env:DSH_HOME) { $env:DSH_HOME } else { Join-Path $env:USERPROFILE '.dsh' }),
    [switch]$Force
)
$ErrorActionPreference = 'Stop'
$src = Join-Path $PSScriptRoot 'dsh-armor.patch.yml'
if (-not (Test-Path $src)) { throw "missing $src" }
New-Item -ItemType Directory -Force -Path $DshHome | Out-Null
$target = Join-Path $DshHome 'cordis.patch.yml'
$companion = Join-Path $DshHome 'cordis.ty-armor.yml'

if (-not (Test-Path $target)) {
    Copy-Item $src $target
    Write-Output ('installed -> ' + $target)
    exit 0
}

# Existing home patch: back up, then merge only when it is a top-level sequence.
$existing = [IO.File]::ReadAllText($target)
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
Copy-Item $target ($target + '.bak-' + $stamp)
Write-Output ('backup -> ' + $target + '.bak-' + $stamp)

$trimmed = $existing.TrimStart([char]0xFEFF, ' ', "`r", "`n", "`t")
if ($trimmed.StartsWith('-')) {
    $ours = [IO.File]::ReadAllText($src)
    # Drop our leading comment block so the merged file keeps one header.
    $oursBody = ($ours -split "`n" | Where-Object { -not $_.StartsWith('#') }) -join "`n"
    [IO.File]::WriteAllText($target, $existing.TrimEnd() + "`r`n`r`n" + $oursBody.TrimStart(), (New-Object System.Text.UTF8Encoding($false)))
    Write-Output ('merged into -> ' + $target)
} else {
    # Not a sequence we can safely merge textually: keep the user file intact.
    Copy-Item $src $companion
    Write-Output ('existing home patch is not a top-level sequence; wrote companion instead -> ' + $companion)
    Write-Output ('apply per run with: dsh web --patch "' + $companion + '"')
}
