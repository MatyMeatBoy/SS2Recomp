param(
    [string]$RecoveryLogDirectory,
    [string]$GameExecutable,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$GameArguments
)
$ErrorActionPreference = 'Stop'
$ss2Root = Split-Path -Parent $PSScriptRoot
$ss2Exe = Join-Path $ss2Root 'build/ShiningSoul2Recomp.exe'
if ($GameExecutable) { $ss2Exe = $GameExecutable }
if (-not (Test-Path -LiteralPath $ss2Exe)) { throw 'No se encuentra ShiningSoul2Recomp.exe.' }
# Windows argv quoting, including literal quotes and trailing backslashes.
function Quote-SS2Argument([string]$Value) {
    if ($Value -notmatch '[\s"]' -and $Value.Length) { return $Value }
    $ss2Quoted = [regex]::Replace($Value, '(\\*)"', '$1$1\"')
    $ss2Quoted = [regex]::Replace($ss2Quoted, '(\\+)$', '$1$1')
    return '"' + $ss2Quoted + '"'
}
$ss2Arguments = @('--netplay-safe', '--screen', 'raw') + $GameArguments
$ss2ArgumentLine = ($ss2Arguments | ForEach-Object { Quote-SS2Argument $_ }) -join ' '
$ss2Attempts = 0
while ($true) {
    $ss2Started = [DateTime]::UtcNow
    $ss2StartOptions = @{ FilePath=$ss2Exe; ArgumentList=$ss2ArgumentLine;
        WorkingDirectory=$ss2Root; WindowStyle='Hidden'; PassThru=$true; Wait=$true }
    if ($RecoveryLogDirectory) {
        New-Item -ItemType Directory -Path $RecoveryLogDirectory -Force | Out-Null
        $ss2StartOptions.RedirectStandardOutput = Join-Path $RecoveryLogDirectory 'game.stdout.log'
        $ss2StartOptions.RedirectStandardError = Join-Path $RecoveryLogDirectory 'game.stderr.log'
    }
    $ss2Child = Start-Process @ss2StartOptions
    $ss2Code = $ss2Child.ExitCode
    # A normal close is intentional. Configuration/runtime validation failures
    # must not loop. Restart only Windows crash termination codes (NTSTATUS).
    if ($ss2Code -ge 0) { break }
    if (([DateTime]::UtcNow - $ss2Started).TotalSeconds -lt 10) { $ss2Attempts++ }
    else { $ss2Attempts = 0 }
    if ($ss2Attempts -ge 3) { throw 'Tres crashes consecutivos; revisar el registro antes de reintentar.' }
    Start-Sleep -Seconds 2
}
exit $ss2Code
