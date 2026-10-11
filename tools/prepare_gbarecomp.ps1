$ErrorActionPreference = 'Stop'

# The application release is already built. This helper aligns the local
# recomp-net submodule with the transport version used by the tested binary
# when rebuilding from source.
$root = Split-Path -Parent $PSScriptRoot
$net = Join-Path $root 'gbarecomp/external/recomp-net'
git -C $net fetch origin bb6a616c4418a1b2209c7083a583bbd31ac7d797 --quiet
git -C $net checkout --detach bb6a616c4418a1b2209c7083a583bbd31ac7d797
Write-Host 'recomp-net pinned to bb6a616c4418a1b2209c7083a583bbd31ac7d797.'
