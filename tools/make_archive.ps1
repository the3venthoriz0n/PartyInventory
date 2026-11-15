param(
    [string]$Version = "0.2.1"
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
if (-not $ascii.Contains("PartyInventory $Version loading")) { throw "Version marker for $Version not found in DLL." }

# Never replace an earlier build: if the target already exists, add a _yyyyMMdd-HHmmss suffix before the extension.
function Get-UniquePath([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return $Path }
    $dir  = Split-Path -Parent $Path
    $base = [System.IO.Path]::GetFileNameWithoutExtension($Path)
    $ext  = [System.IO.Path]::GetExtension($Path)
    return (Join-Path $dir ("{0}_{1}{2}" -f $base, (Get-Date -Format "yyyyMMdd-HHmmss"), $ext))
}

New-Item -ItemType Directory -Path $dist -Force | Out-Null
$zip = Get-UniquePath $zip
Compress-Archive -Path $data -DestinationPath $zip -CompressionLevel Optimal -Force
. (Join-Path $PSScriptRoot "legal_docs.ps1")
Add-LegalDocs -Zip $zip -Prefix "Data/Docs/PartyInventory" -Version $Version

Write-Host ""
Write-Host "Archive written to: $zip"
