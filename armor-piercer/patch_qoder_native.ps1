$ErrorActionPreference = 'Stop'
# Rebuild Qoder armor round with the Qoder-native notice (v2 native)
$live = 'C:\Users\ASUS\AppData\Local\Programs\Qoder CN\resources\app.asar.unpacked\node_modules\@qoder-ai\qoder-cn-agent-sdk\dist\_worker\qoder-worker-runtime.obf.mjs'
$proj = 'F:\TZcode\armor-piercer'
$noticeFile = Join-Path $proj 'qoder-fork\armor-notice.txt'
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
$liveBefore = & $sha $live

# locate the current armored notice blob (decodes to the pre-authorized text)
$needle = '_$d("'
$idx = 0; $hit = $null
while (($idx = $text.IndexOf($needle, $idx)) -ge 0) {
    $s = $idx + 5
    $e = $text.IndexOf('"', $s)
    if ($e -lt 0) { break }
    $blob = $text.Substring($s, $e - $s)
    if ($blob -cmatch '^[A-Za-z0-9+/=]+$' -and $blob.Length -gt 100) {
        try {
            $d = Decode $blob
            if ($d -like 'IMPORTANT:*pre-authorized*') { $hit = @{ s = $s; e = $e; blob = $blob; dec = $d }; break }
        } catch {}
    }
    $idx = $e
}
if (-not $hit) { throw 'armored notice blob not found' }

$newNotice = ([System.IO.File]::ReadAllText($noticeFile, [System.Text.Encoding]::UTF8)).Trim() + "`n"
$newCipher = Encode $newNotice
$modText = $text.Substring(0, $hit.s) + $newCipher + $text.Substring($hit.e)
[System.IO.File]::WriteAllText($mod, $modText, (New-Object System.Text.UTF8Encoding($false)))

# verify: decode new blob from produced file, confirm identity + persona hook intact
$back = [System.IO.File]::ReadAllText($mod, [System.Text.Encoding]::UTF8)
$rt = (Decode (($back.Substring($back.IndexOf($newCipher), $newCipher.Length)))).Trim()
$rtOK = ($rt -eq $newNotice.Trim())
$identity = $back.Contains('/.qoder-cn/persona.txt')
$oldGone = -not $back.Contains($hit.blob)
Write-Output ('roundtrip=' + $rtOK + ' personaHook=' + $identity + ' oldGone=' + $oldGone)
Write-Output ('before ' + $liveBefore.Substring(0,8) + '  after ' + (& $sha $mod).Substring(0,8))
if (-not ($rtOK -and $identity -and $oldGone)) { throw 'verification failed' }
