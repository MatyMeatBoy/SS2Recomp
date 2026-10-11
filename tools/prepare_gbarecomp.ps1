$ErrorActionPreference = 'Stop'

# Align the transport and apply the tracked SS2 runtime changes reproducibly.
$root = Split-Path -Parent $PSScriptRoot
$net = Join-Path $root 'gbarecomp/external/recomp-net'
git -C $net fetch origin bb6a616c4418a1b2209c7083a583bbd31ac7d797 --quiet
if ($LASTEXITCODE -ne 0) { throw 'Cannot fetch the pinned recomp-net revision.' }
git -C $net checkout --detach bb6a616c4418a1b2209c7083a583bbd31ac7d797
Write-Host 'recomp-net pinned to bb6a616c4418a1b2209c7083a583bbd31ac7d797.'
if ($LASTEXITCODE -ne 0) { throw 'Cannot align recomp-net checkout.' }
$framework = Join-Path $root 'gbarecomp'
$patch = Join-Path $PSScriptRoot 'patches/gbarecomp-ss2.patch'
# Preserve unrelated local modifications; accept only an exact patch.
git -C $framework apply --reverse --check $patch 2>$null
if ($LASTEXITCODE -eq 0) {
    Write-Host 'Shining Soul II runtime patch already applied.'
} else {
    git -C $framework apply --check $patch
    if ($LASTEXITCODE -ne 0) { throw 'Runtime patch conflicts with local changes.' }
    git -C $framework apply $patch
    if ($LASTEXITCODE -ne 0) { throw 'Cannot apply runtime patch.' }
    Write-Host 'Shining Soul II runtime patch applied.'
}
$ss2Ui = Join-Path $root 'recomp-ui'
$ss2UiPatch = Join-Path $PSScriptRoot 'patches/recomp-ui-ss2.patch'
git -C $ss2Ui apply --reverse --check $ss2UiPatch 2>$null
if ($LASTEXITCODE -ne 0) {
    git -C $ss2Ui apply --check $ss2UiPatch
    if ($LASTEXITCODE -ne 0) { throw 'Launcher patch conflicts with local changes.' }
    git -C $ss2Ui apply $ss2UiPatch
    if ($LASTEXITCODE -ne 0) { throw 'Cannot apply launcher patch.' }
}
Write-Host 'Shining Soul II experimental launcher patch ready.'
