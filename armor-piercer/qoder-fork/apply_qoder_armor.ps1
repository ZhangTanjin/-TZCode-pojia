param([string]$Original = 'C:\Users\ASUS\AppData\Local\Programs\Qoder CN\resources\app.asar.unpacked\node_modules\@qoder-ai\qoder-cn-agent-sdk\dist\_worker\qoder-worker-runtime.obf.mjs', [string]$Output = 'F:\TZcode\armor-piercer\qoder-fork\qoder-worker-runtime.armor.mjs')
$ErrorActionPreference='Stop'
$key=[System.Text.Encoding]::ASCII.GetBytes('qInfTBqB6D3R')
function Decode([string]$s){$b=[System.Convert]::FromBase64String($s);for($i=0;$i -lt $b.Length;$i++){$b[$i]=$b[$i] -bxor $key[$i%$key.Length]};[System.Text.Encoding]::UTF8.GetString($b)}
function Encode([string]$s){$b=[System.Text.Encoding]::UTF8.GetBytes($s);for($i=0;$i -lt $b.Length;$i++){$b[$i]=$b[$i] -bxor $key[$i%$key.Length]};[System.Convert]::ToBase64String($b)}
$t=[System.IO.File]::ReadAllText($Original,[System.Text.Encoding]::UTF8)
$n='_$d("';$idx=0;$found=$null
while(($idx=$t.IndexOf($n,$idx)) -ge 0){$s=$idx+5;$e=$t.IndexOf('"',$s);if($e -lt 0){break}$b=$t.Substring($s,$e-$s);if($b -cmatch '^[A-Za-z0-9+/=]+$' -and $b.Length -gt 100){try{$d=Decode $b;if($d.StartsWith('IMPORTANT: Assist with authorized security testing')){$found=@{s=$s;e=$e;b=$b};break}}catch{}}$idx=$e}
if(-not $found){throw 'blob not found'}
$armor=[System.IO.File]::ReadAllText('F:\TZcode\armor-piercer\qoder-fork\armor-notice.txt',[System.Text.Encoding]::UTF8)
[System.IO.File]::WriteAllText($Output,$t.Substring(0,$found.s)+(Encode $armor)+$t.Substring($found.e),(New-Object System.Text.UTF8Encoding($false)))
Write-Output ('RECONSTRUCTED ' + (Get-FileHash -Algorithm SHA256 $Output).Hash)