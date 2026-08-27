[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$version = '3.0.0'
$stage = Join-Path $root "build\package\BPR-$version"
$dist = Join-Path $root 'dist'
$archive = Join-Path $dist "Bullet Penetration and Ricochet-$version.zip"
$dll = Join-Path $root 'build\windows\x64\release\BPR.dll'

function Assert-UnderProjectRoot([string]$Path) {
    $prefix = $root.TrimEnd('\') + '\'
    $full = [System.IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to package outside the BPR project root: $full"
    }
    return $full
}

$stage = Assert-UnderProjectRoot $stage
$archive = Assert-UnderProjectRoot $archive
if (-not (Test-Path -LiteralPath $dll -PathType Leaf)) {
    throw "Build the Release DLL before packaging: $dll"
}

if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
New-Item -ItemType Directory -Path $stage | Out-Null
Copy-Item -Path (Join-Path $root 'package\*') -Destination $stage -Recurse -Force
Copy-Item -LiteralPath $dll -Destination (Join-Path $stage 'F4SE\Plugins\BPR.dll') -Force

$licenseDirectory = Join-Path $stage 'Licenses'
New-Item -ItemType Directory -Path $licenseDirectory | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination (Join-Path $licenseDirectory 'GPL-3.0.txt')
Copy-Item -LiteralPath (Join-Path $root 'EXCEPTIONS') -Destination (Join-Path $licenseDirectory 'GPL-3.0-EXCEPTIONS.txt')
Copy-Item -LiteralPath (Join-Path $root 'licenses\PenetrationSystem-MIT.txt') -Destination $licenseDirectory
Copy-Item -LiteralPath (Join-Path $root 'lib\commonlibf4\LICENSE') -Destination (Join-Path $licenseDirectory 'CommonLibF4-MIT.txt')
Copy-Item -LiteralPath (Join-Path $root 'licenses\spdlog-MIT.txt') -Destination (Join-Path $licenseDirectory 'spdlog-MIT.txt')
Copy-Item -LiteralPath (Join-Path $root 'COPYRIGHT.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'THIRD_PARTY_NOTICES.md') -Destination $stage

New-Item -ItemType Directory -Path $dist -Force | Out-Null
if (Test-Path -LiteralPath $archive) {
    Remove-Item -LiteralPath $archive -Force
}
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -CompressionLevel Optimal

$hash = Get-FileHash -LiteralPath $archive -Algorithm SHA256
Write-Host "Created $archive"
Write-Host "SHA256 $($hash.Hash)"
