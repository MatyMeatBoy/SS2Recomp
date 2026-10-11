param(
    [ValidateSet('Host','Guest')][string]$Role = 'Host',
    [ValidateRange(2,4)][int]$Players = 2,
    [ValidateRange(1,3)][int]$Seat = 1,
    [ValidateRange(1,2147483647)][int]$Session = 24680,
    [ValidateRange(1024,65535)][int]$Port = 19740,
    [string]$HostAddress
)
$ErrorActionPreference = 'Stop'
if ($Role -eq 'Guest') {
    if (-not $HostAddress) { $HostAddress = Read-Host 'IP de Hamachi o nombre del host' }
    if ($HostAddress -notmatch '^[a-zA-Z0-9.-]+$') { throw 'Direccion IPv4 o nombre de host no valido.' }
    if ($Seat -ge $Players) { throw 'El asiento debe ser menor que el numero de jugadores.' }
    $ss2Seat = $Seat
} else { $ss2Seat = 0 }
$ss2RoomArgs = @('--netplay-players', "$Players", '--netplay-seat', "$ss2Seat",
    '--netplay-session', "$Session", '--netplay-bind', "0.0.0.0:$Port", '--netplay-view', '16:9')
if ($Role -eq 'Guest') { $ss2RoomArgs += @('--netplay-peer', "${HostAddress}:$Port") }
& (Join-Path $PSScriptRoot 'run_recoverable.ps1') @ss2RoomArgs
