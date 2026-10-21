$ErrorActionPreference = 'Stop'
$proj = 'F:\TZcode\armor-piercer'
$vcvars = 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'
$sha = { param($p) (Get-FileHash -Algorithm SHA256 $p).Hash }

# weapon exe name built from char codes: zcode + 0x7834 0x7532 0x5F39 + .exe  (zcode <po-jia-dan>.exe)
$cn = 'zcode' + [char]0x7834 + [char]0x7532 + [char]0x5F39
$live = Join-Path $proj ($cn + '.exe')
$bak  = Join-Path $proj ($cn + '.pre-fusion.bak.exe')

if (-not (Test-Path $vcvars)) { throw "vcvars64.bat not found at $vcvars" }

# 1) build (already done in prior run if piercer.exe fresh; rebuild anyway for a clean ledger)
$buildLog = Join-Path $proj 'build3.fusion.out'
cmd /c (Join-Path $proj 'build3.bat') > $buildLog 2>&1
$buildTxt = [System.IO.File]::ReadAllText($buildLog)
if ($buildTxt -notmatch '\[OK\] build success') { throw 'build failed - see build3.fusion.out' }

# 2) verify fresh piercer.exe carries the fused round
$piercer = Join-Path $proj 'piercer.exe'
if (-not (Test-Path $piercer)) { throw 'piercer.exe missing' }
$latin = [System.Text.Encoding]::GetEncoding(28591).GetString([System.IO.File]::ReadAllBytes($piercer))
$hasLaw  = $latin.Contains('ATOMIC CONTINUATION DISPATCH')
$hasHook = $latin.Contains('ZCODE_PERSONA_FILE')
if (-not ($hasLaw -and $hasHook)) { throw "piercer.exe markers missing: law=$hasLaw hook=$hasHook" }

# 3) swap the live weapon (old exe may or may not be running)
try { & taskkill /F /IM ($cn + '.exe') 2>&1 | Out-Null } catch { }
Start-Sleep -Milliseconds 800
if (Test-Path $live) { Copy-Item -Force $live $bak }
Copy-Item -Force $piercer $live

# 4) final verification
$liveLatin = [System.Text.Encoding]::GetEncoding(28591).GetString([System.IO.File]::ReadAllBytes($live))
$liveLaw  = $liveLatin.Contains('ATOMIC CONTINUATION DISPATCH')
$liveHook = $liveLatin.Contains('ZCODE_PERSONA_FILE')
$payloadHash = & $sha (Join-Path $proj 'armor_payload.bin')
$liveHash = & $sha $live
$bakHash  = & $sha $bak

$v = @()
$v += 'WEAPON UPDATE - fused round loaded into the piercer exe'
$v += ('weapon exe     : ' + $live)
$v += ('vcvars         : ' + $vcvars)
$v += ('build          : [OK] build success (build3.fusion.out)')
$v += ('armor_payload.bin sha256 : ' + $payloadHash)
$v += ('piercer.exe    : law=' + $hasLaw + ' hook=' + $hasHook)
$v += ('live exe sha256: ' + $liveHash)
$v += ('live markers   : law=' + $liveLaw + ' hook=' + $liveHook)
$v += ('pre-fusion exe backup : ' + $bak + '  sha256=' + $bakHash)
$v += ''
$v += 'ROLLBACK: close the weapon exe, copy the .pre-fusion.bak.exe back over the live exe name,'
$v += '          optionally copy armor_payload.pre-fusion.bin over armor_payload.bin. No rebuild needed.'
$v += 'DEPLOY to ZCode: launch the weapon exe and press deploy - it writes the fused zcode.cjs'
$v += '          into Program Files (closes ZCode first, admin manifest built in).'
[System.IO.File]::WriteAllText((Join-Path $proj 'VERIFICATION_WEAPON.txt'), ($v -join "`r`n"), (New-Object System.Text.UTF8Encoding($true)))

Write-Output ('live law=' + $liveLaw + ' hook=' + $liveHook)
Write-Output ('live sha256=' + $liveHash)
Write-Output ('backup sha256=' + $bakHash)
Write-Output 'WEAPON UPDATED'
