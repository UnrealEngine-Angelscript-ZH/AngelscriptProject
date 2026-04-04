param(
    [int]$Count = 3,
    [int]$DelayMs = 200
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

for ($index = 1; $index -le $Count; $index++) {
    Write-Output ("tick:{0}" -f $index)
    Start-Sleep -Milliseconds $DelayMs
}
