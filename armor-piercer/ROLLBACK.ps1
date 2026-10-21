param([Parameter(Mandatory=$true)][string]$TargetCopy)
$basis = Join-Path $PSScriptRoot 'armor_payload.bin'
$want = (Get-FileHash -Algorithm SHA256 $basis).Hash
Copy-Item -Force $basis $TargetCopy
$got = (Get-FileHash -Algorithm SHA256 $TargetCopy).Hash
if ($got -eq $want) { Write-Output ("ROLLBACK OK " + $got); exit 0 } else { Write-Output ("ROLLBACK MISMATCH want=$want got=$got"); exit 1 }