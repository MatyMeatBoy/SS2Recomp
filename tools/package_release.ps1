param([string]$Version = '0.2.0')
$ErrorActionPreference = 'Stop'
$ss2Root = Split-Path -Parent $PSScriptRoot
$ss2Name = "ShiningSoul2Recomp-v$Version-windows-x64"
$ss2Stage = Join-Path $ss2Root "release-stage/$ss2Name"
if (Test-Path -LiteralPath $ss2Stage) { throw 'Release staging directory already exists.' }
New-Item -ItemType Directory -Path $ss2Stage | Out-Null
$ss2Files = @(
 'Jugar ShiningSoul2Recomp.bat', 'Jugar ShiningSoul2Recomp Online Seguro.bat',
 'Online Host.bat', 'Online Invitado.bat', 'README.md', 'LICENSE',
 'tools/run_recoverable.ps1', 'tools/online_room.ps1',
 'docs/NETPLAY_RECOVERY.md', 'docs/NETPLAY_VALIDATION.md', 'docs/recovery-v0.2.0-results.json',
 'docs/RELEASE_v0.2.0.md', 'build/ShiningSoul2Recomp.exe', 'build/SDL2.dll',
 'build/mods/state.toml',
 'build/mods/packages/shining-soul-2.enhancement.adaptive-view/1.0.0/manifest.toml'
)
foreach ($ss2File in $ss2Files) {
 $ss2Source = Join-Path $ss2Root $ss2File
 $ss2Destination = Join-Path $ss2Stage $ss2File
 New-Item -ItemType Directory -Path (Split-Path -Parent $ss2Destination) -Force | Out-Null
 Copy-Item -LiteralPath $ss2Source -Destination $ss2Destination
}
Copy-Item -LiteralPath (Join-Path $ss2Root 'docs/RELEASE_v0.2.0.md') -Destination (Join-Path $ss2Stage 'RELEASE_NOTES.md')
$ss2Licenses = @{
 'gbarecomp/LICENSE' = 'gbarecomp.txt'
 'recomp-ui/LICENSE' = 'recomp-ui.txt'
 'gbarecomp/external/recomp-net/LICENSE' = 'recomp-net.txt'
 'gbarecomp/external/rbengine/LICENSE' = 'rbengine.txt'
 'gbarecomp/external/arm-recomp-core/LICENSE' = 'arm-recomp-core.txt'
}
New-Item -ItemType Directory -Path (Join-Path $ss2Stage 'licenses') | Out-Null
foreach ($ss2License in $ss2Licenses.GetEnumerator()) {
 Copy-Item -LiteralPath (Join-Path $ss2Root $ss2License.Key) -Destination (Join-Path $ss2Stage "licenses/$($ss2License.Value)")
}
$ss2SdlHeader = Join-Path $ss2Root '../PrimatesGBA/deps/SDL2-2.32.10/x86_64-w64-mingw32/include/SDL2/SDL.h'
$ss2SdlNotice = (Get-Content -LiteralPath $ss2SdlHeader -TotalCount 20) -join "`n"
[IO.File]::WriteAllText((Join-Path $ss2Stage 'licenses/SDL2.txt'), "$ss2SdlNotice`n")
$ss2Zip = Join-Path $ss2Root "release-stage/$ss2Name.zip"
Compress-Archive -LiteralPath $ss2Stage -DestinationPath $ss2Zip
$ss2Hash = (Get-FileHash -LiteralPath $ss2Zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $ss2Root "release-stage/$ss2Name.sha256"), "$ss2Hash  $ss2Name.zip`n")
Write-Output $ss2Zip
