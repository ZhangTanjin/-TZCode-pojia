param([Parameter(Mandatory=$true)][string]$TargetCopy)
$basis = 'C:\Users\ASUS\AppData\Local\Programs\Qoder CN\resources\app.asar.unpacked\node_modules\@qoder-ai\qoder-cn-agent-sdk\dist\_worker\qoder-worker-runtime.obf.mjs.official.bak'
$want = (Get-FileHash -Algorithm SHA256 $basis).Hash
Copy-Item -Force $basis $TargetCopy
$got = (Get-FileHash -Algorithm SHA256 $TargetCopy).Hash
if ($got -eq $want) { Write-Output ('ROLLBACK OK ' + $got); exit 0 } else { Write-Output ('ROLLBACK MISMATCH want=' + $want + ' got=' + $got); exit 1 }