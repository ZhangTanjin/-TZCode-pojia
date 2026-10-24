param(
    [string]$Worker,
    [string]$Out,
    [switch]$Deploy
)
$ErrorActionPreference = 'Stop'
$proj = 'F:\TZcode\armor-piercer'
$fork = Join-Path $proj 'qoder-fork'
$sha = { param($p) (Get-FileHash -Algorithm SHA256 $p).Hash }

# ---- auto-detect the newest installed worker (0.4.9+ uses .qoder-versions) ----
if (-not $Worker) {
    $root = Join-Path $env:LOCALAPPDATA 'Programs\Qoder CN'
    $cands = @()
    $verRoot = Join-Path $root '.qoder-versions'
    if (Test-Path $verRoot) {
        $cands += Get-ChildItem $verRoot -Directory | Sort-Object { [version]$_.Name } -Descending | ForEach-Object {
            Join-Path $_.FullName 'resources\app.asar.unpacked\node_modules\@qoder-ai\qoder-cn-agent-sdk\dist\_worker\qoder-worker-runtime.obf.mjs'
        }
    }
    $cands += Join-Path $root 'resources\app.asar.unpacked\node_modules\@qoder-ai\qoder-cn-agent-sdk\dist\_worker\qoder-worker-runtime.obf.mjs'
    $Worker = $cands | Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $Worker) { throw 'no Qoder worker runtime found' }
}
if (-not $Out) { $Out = Join-Path $proj 'qoder_armor_payload.mjs' }
Write-Output ('worker: ' + $Worker)
$verTag = if ($Worker -match '\.qoder-versions\\([^\\]+)\\') { $Matches[1] } else { 'legacy' }

$text = [System.IO.File]::ReadAllText($Worker, [System.Text.Encoding]::UTF8)
$before = & $sha $Worker

# ---- detect the XOR key from the header decoder ----
$m = [regex]::Match($text.Substring(0, [Math]::Min(400, $text.Length)), 'const _\$d=\(s,k="([^"]+)"\)')
if (-not $m.Success) { throw 'decoder signature not found' }
$keyStr = $m.Groups[1].Value
$key = [System.Text.Encoding]::ASCII.GetBytes($keyStr)
Write-Output ('key: ' + $keyStr)
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

# ---- identity hook (idempotent) ----
$iife = '(()=>{try{const __p=require("node:fs").readFileSync(require("node:os").homedir()+"/.qoder-cn/persona.txt","utf8").trim();if(__p)return __p}catch(__e){}return '
if (-not $text.Contains('/.qoder-cn/persona.txt')) {
    $bt = [char]96
    $p1 = '([A-Za-z_$][\w$]*)="You are a Qoder agent\."'
    $m1 = [regex]::Match($text, $p1)
    if (-not $m1.Success) { throw 'identity constant (You are a Qoder agent.) not found' }
    $text = $text.Substring(0, $m1.Index) + $m1.Groups[1].Value + '=' + $iife + '"You are a Qoder agent."})()' + $text.Substring($m1.Index + $m1.Length)
    # template identity: X=<bt>You are ${Y}.<bt>
    $p2 = '([A-Za-z_$][\w$]*)=' + $bt + 'You are \$\{([A-Za-z_$][\w$]*)\}\.' + $bt
    $m2 = [regex]::Match($text, $p2)
    if ($m2.Success) {
        $text = $text.Substring(0, $m2.Index) + $m2.Groups[1].Value + '=' + $iife + $m2.Value.Substring($m2.Value.IndexOf('=') + 1) + '})()' + $text.Substring($m2.Index + $m2.Length)
        Write-Output 'identity: both constants hooked'
    } else {
        Write-Output 'identity: primary hooked (template constant not found)'
    }
} else {
    Write-Output 'identity: hook already present'
}

# ---- collect blobs and patch by content predicate ----
$specs = @(
    @{ name = 'notice';     file = 'armor-notice.txt';   pred = { param($d) $d.StartsWith('IMPORTANT:') } },
    @{ name = 'delivering'; file = 'layer-delivering.txt'; pred = { param($d) $d.StartsWith('# Delivering work') } },
    @{ name = 'classifier'; file = 'layer-classifier.txt'; pred = { param($d) $d.StartsWith('Judge whether the pending action') } },
    @{ name = 'rules';      file = 'layer-rules.txt';      pred = { param($d) $d.Contains('CHECK: Is this routine') } }
)
$needle = '_$d("'
$hits = @{}
$idx = 0
while (($idx = $text.IndexOf($needle, $idx)) -ge 0) {
    $s = $idx + 5; $e = $text.IndexOf('"', $s); if ($e -lt 0) { break }
    $blob = $text.Substring($s, $e - $s)
    if ($blob -cmatch '^[A-Za-z0-9+/=]+$' -and $blob.Length -gt 300) {
        try { $d = Decode $blob
            foreach ($spec in $specs) {
                if (-not $hits.ContainsKey($spec.name) -and (& $spec.pred $d)) {
                    $hits[$spec.name] = [pscustomobject]@{ s = $s; e = $e; old = $blob; dec = $d; file = $spec.file }
                }
            }
        } catch {}
    }
    $idx = $e
}
foreach ($name in @('notice','delivering','classifier','rules')) {
    if (-not $hits.ContainsKey($name)) { throw ('target blob not found: ' + $name) }
}

$edits = @()
foreach ($name in @('notice','delivering','classifier','rules')) {
    $h = $hits[$name]
    $newText = ([System.IO.File]::ReadAllText((Join-Path $fork $h.file), [System.Text.Encoding]::UTF8)).Trim() + "`n"
    $edits += [pscustomobject]@{ name = $name; s = $h.s; e = $h.e; old = $h.old; text = $newText; new = (Encode $newText) }
    Write-Output ('  ' + $name.PadRight(11) + ' old=' + $h.dec.Length + '  new=' + $newText.Length)
}
foreach ($ed in ($edits | Sort-Object s -Descending)) {
    $text = $text.Substring(0, $ed.s) + $ed.new + $text.Substring($ed.e)
}

# ---- write payload ----
[System.IO.File]::WriteAllText($Out, $text, (New-Object System.Text.UTF8Encoding($false)))
$after = & $sha $Out

# ---- verify ----
$back = [System.IO.File]::ReadAllText($Out, [System.Text.Encoding]::UTF8)
$ok = $true
foreach ($ed in $edits) {
    $rt = (Decode $ed.new).Trim()
    $pass = ($rt -eq $ed.text.Trim()) -and (-not $back.Contains($ed.old))
    Write-Output ("  verify " + $ed.name.PadRight(11) + $pass + "  rtLen=" + $rt.Length + " tgtLen=" + $ed.text.Trim().Length + " oldStillPresent=" + $back.Contains($ed.old))
    if (-not $pass) { $ok = $false }
}
$hook = $back.Contains('/.qoder-cn/persona.txt')
$fb1 = $back.Contains('return "You are a Qoder agent."})()')
$fb2 = $back.Contains('}().') -or $back.Contains('$.`})()') -or $back.Contains('})()')
Write-Output ('  persona hook=' + $hook + '  identity fallback1=' + $fb1)
Write-Output ('version=' + $verTag + '  before=' + $before.Substring(0,8) + '  after=' + $after.Substring(0,8))
if (-not ($ok -and $hook)) { throw 'verification failed' }

if ($Deploy) {
    $bak = $Worker + '.official.bak-' + $verTag
    if (-not (Test-Path $bak)) { Copy-Item $Worker $bak }
    Copy-Item -Force $Out $Worker
    Write-Output ('deployed -> ' + $Worker)
    Write-Output ('live      ' + (& $sha $Worker))
}
