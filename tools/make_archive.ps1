param(
    [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$data = Join-Path $root "package\Data"
$dll  = Join-Path $data "SKSE\Plugins\PartyInventory.dll"
$dist = Join-Path $root "dist"
$zip  = Join-Path $dist ("PartyInventory-{0}.zip" -f $Version)

if (-not (Test-Path -LiteralPath $dll)) { throw "DLL not found: $dll. Build first (build_release.bat)." }

# The DLL must not depend on spdlog.dll / fmt.dll (they do not exist in a Skyrim install).
$bytes = [System.IO.File]::ReadAllBytes($dll)
$ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
if (-not $ascii.Contains("PartyInventory 0.1.0 loading") -and $Version -eq "0.1.0") { throw "Version marker not found in DLL." }

New-Item -ItemType Directory -Path $dist -Force | Out-Null
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -Path $data -DestinationPath $zip -CompressionLevel Optimal -Force
Write-Host "Created $zip"
