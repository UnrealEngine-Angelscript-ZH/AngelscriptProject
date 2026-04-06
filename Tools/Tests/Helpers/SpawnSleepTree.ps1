param(
    [Parameter(Mandatory = $true)]
    [string]$Marker,
    [int]$Seconds = 30
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$powerShell = (Get-Command powershell.exe -ErrorAction Stop).Source
$childCommand = "Write-Output '$Marker child-start'; Start-Sleep -Seconds $Seconds"
$child = Start-Process -FilePath $powerShell -ArgumentList @(
    '-NoProfile'
    '-Command'
    $childCommand
) -PassThru -WindowStyle Hidden

Write-Output "$Marker parent-start $($child.Id)"
Start-Sleep -Seconds $Seconds
