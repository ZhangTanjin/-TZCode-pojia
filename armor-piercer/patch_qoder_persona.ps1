$ErrorActionPreference = 'Stop'
# ---- Phase 2: persona-loader hook on identity constants (on top of armored v1) ----
$live = 'C:\Users\ASUS\AppData\Local\Programs\Qoder CN\resources\app.asar.unpacked\node_modules\@qoder-ai\qoder-cn-agent-sdk\dist\_worker\qoder-worker-runtime.obf.mjs'
$proj = 'F:\TZcode\armor-piercer\qoder-fork'
$mod  = Join-Path $proj 'qoder-worker-runtime.armor2.mjs'
$ver  = Join-Path $proj 'VERIFICATION_QODER_P2.txt'
$sha  = { param($p) (Get-FileHash -Algorithm SHA256 $p).Hash }

$iife = '(()=>{try{const __p=require("node:fs").readFileSync(require("node:os").homedir()+"/.qoder-cn/persona.txt","utf8").trim();if(__p)return __p}catch(__e){}return '

$text = [System.IO.File]::ReadAllText($live, [System.Text.Encoding]::UTF8)
$liveHash = & $sha $live

$n1 = 'Opl="You are a Qoder agent."'
$r1 = 'Opl=' + $iife + '"You are a Qoder agent."})()'
$n2 = 'Hpl=`You are ${rr}.`'
$r2 = 'Hpl=' + $iife + '`You are ${rr}.`})()'

foreach ($pair in @(@($n1,$r1), @($n2,$r2))) {
    $n = $pair[0]
    $c = ([regex]::Matches($text, [regex]::Escape($n))).Count
    if ($c -ne 1) { throw ('needle count ' + $c + ' for: ' + $n) }
}
if ($text.Contains('__qoderPersonaLoaded')) { throw 'already patched' }

$modText = $text.Replace($n1, $r1).Replace($n2, $r2)
[System.IO.File]::WriteAllText($mod, $modText, (New-Object System.Text.UTF8Encoding($false)))
$modHash = & $sha $mod

# verify
$back = [System.IO.File]::ReadAllText($mod, [System.Text.Encoding]::UTF8)
$v1 = $back.Contains('Opl=(()=>{try{const __p=require("node:fs")')
$v2 = $back.Contains('return "You are a Qoder agent."})()')
$v3 = $back.Contains('return `You are ${rr}.`})()')
$v4 = $back.Contains('/.qoder-cn/persona.txt')
$v5 = -not $back.Contains($n1)
$v6 = $back.Contains('You are a Qoder agent.')  # fallback intact

$v = @()
$v += 'VERIFIED TRANSACTION P2 - Qoder persona loader (identity layer)'
$v += ('TARGET   : ' + $live + '  (armored v1, AE19A3C2...)')
$v += ('needle 1 : ' + $n1)
$v += ('needle 2 : ' + $n2)
$v += ('live  sha256 : ' + $liveHash)
$v += ('mod   sha256 : ' + $modHash)
$v += ('checks: hook1=' + $v1 + ' fb1=' + $v2 + ' fb2=' + $v3 + ' path=' + $v4 + ' old-gone=' + $v5 + ' fallback-intact=' + $v6)
$v += ''
$v += 'ROLLBACK: re-run apply_qoder_armor.ps1 -Original <.mjs.official.bak> to restore armored v1,'
$v += '          or copy .mjs.official.bak for full factory reset.'
[System.IO.File]::WriteAllText($ver, ($v -join "`r`n"), (New-Object System.Text.UTF8Encoding($true)))
Write-Output ('mod ' + $modHash)
Write-Output ('hook1=' + $v1 + ' fb1=' + $v2 + ' fb2=' + $v3 + ' path=' + $v4 + ' old-gone=' + $v5 + ' fallback=' + $v6)
