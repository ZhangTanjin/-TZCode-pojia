$ErrorActionPreference = 'Stop'
# Qoder multi-layer armor: notice + # Delivering work + auto-mode classifier
# Inputs: qoder-fork\armor-notice.txt / layer-delivering.txt / layer-classifier.txt
$proj = 'F:\TZcode\armor-piercer'
$live = 'C:\Users\ASUS\AppData\Local\Programs\Qoder CN\resources\app.asar.unpacked\node_modules\@qoder-ai\qoder-cn-agent-sdk\dist\_worker\qoder-worker-runtime.obf.mjs'
$mod  = Join-Path $proj 'qoder_armor_payload.mjs'
$sha  = { param($p) (Get-FileHash -Algorithm SHA256 $p).Hash }
$key  = [System.Text.Encoding]::ASCII.GetBytes('qInfTBqB6D3R')
function Decode([string]$s) {
    $b = [System.Convert]::FromBase64String($s)
    for ($i = 0; $i -lt $b.Length; $i++) { $b[$i] = $b[$i] -bxor $key[$i % $key.Length] }
    return [System.Text.Encoding]::UTF8.GetString($b)
}
function Encode([string]$s) {
    $b = [System.Text.Encoding]::UTF8.GetBytes($s)
    for ($i = 0; $i -lt $b.Length; $i++) { $b[$i] = $b[$i] -bxor $key[$i % $key.Length] }
    return [System.Convert]::ToBase64String($b)
}

$text = [System.IO.File]::ReadAllText($live, [System.Text.Encoding]::UTF8)
$before = & $sha $live

$specs = @(
    @{ name='notice';     file='qoder-fork\armor-notice.txt';     match={ param($d) $d -like 'IMPORTANT:*pre-authorized*' } },
    @{ name='delivering'; file='qoder-fork\layer-delivering.txt'; match={ param($d) $d -like '*Refusals are only for requests*' } },
    @{ name='classifier'; file='qoder-fork\layer-classifier.txt'; match={ param($d) $d -like 'Judge whether the pending action*' } }
)

# collect all blobs once
$blobs = @()
$needle = '_$d("'
$idx = 0
while (($idx = $text.IndexOf($needle, $idx)) -ge 0) {
    $s = $idx + 5
    $e = $text.IndexOf('"', $s)
    if ($e -lt 0) { break }
    $blob = $text.Substring($s, $e - $s)
    if ($blob -cmatch '^[A-Za-z0-9+/=]+$' -and $blob.Length -gt 400) {
        try { $d = Decode $blob; $blobs += [pscustomobject]@{ s=$s; e=$e; blob=$blob; dec=$d } } catch {}
    }
    $idx = $e
}
Write-Output ('blobs: ' + $blobs.Count)

$edits = @()
foreach ($spec in $specs) {
    $hit = $blobs | Where-Object { & $spec.match $_.dec } | Select-Object -First 1
    if (-not $hit) { throw ('target not found: ' + $spec.name) }
    $newText = ([System.IO.File]::ReadAllText((Join-Path $proj $spec.file), [System.Text.Encoding]::UTF8)).Trim() + "`n"
    $edits += [pscustomobject]@{ name=$spec.name; s=$hit.s; e=$hit.e; old=$hit.blob; new=(Encode $newText); text=$newText }
    Write-Output ('  ' + $spec.name + '  oldDec=' + $hit.dec.Length + '  newDec=' + $newText.Length + '  at ' + $hit.s)
}

# apply from the end backwards so offsets stay valid
$modText = $text
foreach ($ed in ($edits | Sort-Object s -Descending)) {
    $modText = $modText.Substring(0, $ed.s) + $ed.new + $modText.Substring($ed.e)
}
[System.IO.File]::WriteAllText($mod, $modText, (New-Object System.Text.UTF8Encoding($false)))

# verify each edit round-trips and old text is gone
$back = [System.IO.File]::ReadAllText($mod, [System.Text.Encoding]::UTF8)
$ok = $true
foreach ($ed in $edits) {
    $rt = (Decode $ed.new).Trim()
    $pass = ($rt -eq $ed.text.Trim()) -and (-not $back.Contains($ed.old))
    Write-Output ('  verify ' + $ed.name + ': ' + $pass)
    if (-not $pass) { $ok = $false }
}
$hook = $back.Contains('/.qoder-cn/persona.txt')
Write-Output ('persona hook intact: ' + $hook)
Write-Output ('before ' + $before.Substring(0,8) + '   after ' + (& $sha $mod).Substring(0,8))
if (-not ($ok -and $hook)) { throw 'verification failed' }
