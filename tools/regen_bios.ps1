param(
    [string]$Bios = (Join-Path $PSScriptRoot '..\gbarecomp\bios\gba_bios.bin'),
    [string]$GbarecompRoot = (Join-Path $PSScriptRoot '..\gbarecomp'),
    [string]$Out = (Join-Path $PSScriptRoot '..\build\generated_bios')
)

# Recompiles the user's own GBA BIOS dump into build-local sources. The BIOS
# is never downloaded or committed; it must hash-verify before use.
$ErrorActionPreference = 'Stop'
$biosPath = (Resolve-Path $Bios).Path
$engine = (Resolve-Path $GbarecompRoot).Path
$tool = Join-Path $engine 'build\gba_recompile.exe'
if (-not (Test-Path -LiteralPath $tool)) {
    throw "Missing $tool. Build the engine's gba_recompile target first."
}

$actual = (Get-FileHash -LiteralPath $biosPath -Algorithm SHA1).Hash.ToLowerInvariant()
$expected = '300c20df6731a33952ded8c436f7f186d25d3492'
if ($actual -ne $expected) {
    throw "GBA BIOS SHA-1 mismatch: got $actual expected $expected"
}

New-Item -ItemType Directory -Force -Path $Out | Out-Null
& $tool --bios $biosPath --config (Join-Path $engine 'bios\gba_bios.toml') --out $Out
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
