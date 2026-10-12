param([string]$Version = '0.2.2', [string]$BuiltExecutable = 'build/netplay-testing/ShiningSoul2Recomp.exe')
$ErrorActionPreference = 'Stop'
$ss2Root = Split-Path -Parent $PSScriptRoot
$ss2Name = "ShiningSoul2Recomp-v$Version-windows-x64"
$ss2Stage = Join-Path $ss2Root "release-stage/$ss2Name"
if (Test-Path -LiteralPath $ss2Stage) { throw 'Release staging directory already exists.' }
New-Item -ItemType Directory -Path $ss2Stage | Out-Null
$ss2Files = @(
 'Jugar ShiningSoul2Recomp.bat', 'README.md', 'LICENSE',
 'tools/run_recoverable.ps1',
 'docs/NETPLAY_RECOVERY.md', 'docs/NETPLAY_VALIDATION.md', 'docs/recovery-v0.2.0-results.json', 'docs/recovery-v0.2.1-results.json', 'docs/recovery-v0.2.2-results.json',
 "docs/RELEASE_v$Version.md", 'recomp/launcher/NOTICE.md', 'build/ShiningSoul2Recomp.exe', 'build/SDL2.dll',
 'docs/EQUIPMENT_VIEW.md', 'docs/equipment-view-results.json',
 'mods/preloaded/packages/shining-soul-2.enhancement.adaptive-view/1.0.0/manifest.toml',
 'mods/preloaded/packages/shining-soul-2.enhancement.equipment-view/1.0.0/manifest.toml'
)
foreach ($ss2File in $ss2Files) {
 $ss2Source = Join-Path $ss2Root $ss2File
 if ($ss2File -eq 'build/ShiningSoul2Recomp.exe') { $ss2Source = Join-Path $ss2Root $BuiltExecutable }
 $ss2Destination = Join-Path $ss2Stage $ss2File
 if ($ss2File.StartsWith('mods/preloaded/')) {
  $ss2Destination = Join-Path $ss2Stage ('build/mods/' + $ss2File.Substring('mods/preloaded/'.Length))
 }
 New-Item -ItemType Directory -Path (Split-Path -Parent $ss2Destination) -Force | Out-Null
 Copy-Item -LiteralPath $ss2Source -Destination $ss2Destination
}
Copy-Item -LiteralPath (Join-Path $ss2Root "docs/RELEASE_v$Version.md") -Destination (Join-Path $ss2Stage 'RELEASE_NOTES.md')
$ss2AssetRoot = Join-Path (Split-Path -Parent (Join-Path $ss2Root $BuiltExecutable)) 'assets'
foreach ($ss2Asset in (Get-ChildItem -LiteralPath $ss2AssetRoot -Recurse -File)) {
 if ($ss2Asset.Extension -notin @('.tga','.png','.ttf','.md')) { throw "Unexpected launcher asset: $($ss2Asset.Name)" }
 $ss2AssetRelative = $ss2Asset.FullName.Substring($ss2AssetRoot.Length + 1)
 $ss2AssetDestination = Join-Path $ss2Stage "build/assets/$ss2AssetRelative"
 New-Item -ItemType Directory -Path (Split-Path -Parent $ss2AssetDestination) -Force | Out-Null
 Copy-Item -LiteralPath $ss2Asset.FullName -Destination $ss2AssetDestination
}
$ss2Licenses = @{
 'gbarecomp/LICENSE' = 'gbarecomp.txt'
 'recomp-ui/LICENSE' = 'recomp-ui.txt'
 'gbarecomp/external/recomp-net/LICENSE' = 'recomp-net.txt'
 'gbarecomp/external/rbengine/LICENSE' = 'rbengine.txt'
 'gbarecomp/external/arm-recomp-core/LICENSE' = 'arm-recomp-core.txt'
 'build/_deps/libjuice-src/LICENSE' = 'libjuice.txt'
}
New-Item -ItemType Directory -Path (Join-Path $ss2Stage 'licenses') | Out-Null
foreach ($ss2License in $ss2Licenses.GetEnumerator()) {
 Copy-Item -LiteralPath (Join-Path $ss2Root $ss2License.Key) -Destination (Join-Path $ss2Stage "licenses/$($ss2License.Value)")
}
[IO.File]::WriteAllText((Join-Path $ss2Stage 'licenses/libjuice-source.txt'), "libjuice v1.7.2, unmodified source (MPL-2.0): https://github.com/paullouisageneau/libjuice/tree/v1.7.2`nSource archive: https://github.com/paullouisageneau/libjuice/archive/refs/tags/v1.7.2.tar.gz`nBuild configuration: RNET_ENABLE_ICE=ON, RNET_ICE_BUNDLE_STATIC=ON. See the game repository CMakeLists.txt.`n")
$ss2SdlHeader = Join-Path $ss2Root '../PrimatesGBA/deps/SDL2-2.32.10/x86_64-w64-mingw32/include/SDL2/SDL.h'
$ss2SdlNotice = (Get-Content -LiteralPath $ss2SdlHeader -TotalCount 20) -join "`n"
[IO.File]::WriteAllText((Join-Path $ss2Stage 'licenses/SDL2.txt'), "$ss2SdlNotice`n")
$ss2Zip = Join-Path $ss2Root "release-stage/$ss2Name.zip"
Compress-Archive -LiteralPath $ss2Stage -DestinationPath $ss2Zip
$ss2Hash = (Get-FileHash -LiteralPath $ss2Zip -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $ss2Root "release-stage/$ss2Name.sha256"), "$ss2Hash  $ss2Name.zip`n")
Write-Output $ss2Zip
