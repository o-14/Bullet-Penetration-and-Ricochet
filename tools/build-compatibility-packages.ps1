[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Split-Path -Parent $PSScriptRoot)).Path
$version = '3.1.0'
$dist = Join-Path $root 'dist'
$packageRoot = Join-Path $root "build\package\compatibility-$version"
$packages = [ordered]@{
    'Armament' = 'compatibility\Armament\F4SE\Plugins\BPR\90-Armament.ini'
    'Munitions' = 'compatibility\Munitions\F4SE\Plugins\BPR\90-Munitions.ini'
    'Caliber-Complex-V3' = 'compatibility\Caliber-Complex-V3\F4SE\Plugins\BPR\90-Caliber-Complex-V3.ini'
}

function Assert-UnderProjectRoot([string]$Path) {
    $full = [System.IO.Path]::GetFullPath($Path)
    if (-not $full.StartsWith($root.TrimEnd('\') + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to package outside the BPR project root: $full"
    }
    return $full
}

$packageRoot = Assert-UnderProjectRoot $packageRoot
if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
New-Item -ItemType Directory -Path $dist -Force | Out-Null

foreach ($entry in $packages.GetEnumerator()) {
    $source = Join-Path $root $entry.Value
    if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
        throw "Missing compatibility file: $source"
    }
    $stage = Join-Path $packageRoot $entry.Key
    $targetDirectory = Join-Path $stage 'F4SE\Plugins\BPR'
    New-Item -ItemType Directory -Path $targetDirectory -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $targetDirectory

    $archive = Assert-UnderProjectRoot (Join-Path $dist "BPR-$version-$($entry.Key)-Patch.zip")
    if (Test-Path -LiteralPath $archive) {
        Remove-Item -LiteralPath $archive -Force
    }
    Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -CompressionLevel Optimal
    $hash = Get-FileHash -LiteralPath $archive -Algorithm SHA256
    Write-Host "Created $archive"
    Write-Host "SHA256 $($hash.Hash)"
}
