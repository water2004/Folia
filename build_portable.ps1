[CmdletBinding()]
param(
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string]$Version = '0.8.0',

    [ValidateSet('x64')]
    [string]$Platform = 'x64',

    [switch]$SkipDependencySetup,

    [switch]$SkipStartupTest,

    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$ProgramRoot = Join-Path $PSScriptRoot "build\app-winui\bin\portable\$Platform\Release"
$AssetsRoot = Join-Path $PSScriptRoot 'src\app-winui\Assets'
$OutputDirectory = Join-Path $PSScriptRoot 'build\portable'
$Archive = Join-Path $OutputDirectory "Folia-$Version-$Platform-portable.zip"

if (-not $SkipDependencySetup) { & (Join-Path $PSScriptRoot 'setup.ps1') }
& (Join-Path $PSScriptRoot 'build_app.ps1') -Configuration Release -Platform $Platform -Portable -Clean:$Clean

$Executable = Get-Item -LiteralPath (Join-Path $ProgramRoot 'Folia.exe')
if ($Executable.VersionInfo.ProductVersion -ne $Version) {
    throw "Folia.exe has product version $($Executable.VersionInfo.ProductVersion), expected $Version."
}

. (Join-Path $PSScriptRoot 'tools\Get-DeploymentFiles.ps1')
$ProgramFiles = @(Get-DeploymentFiles -Root $ProgramRoot -Kind Program)
$AssetFiles = @(Get-DeploymentFiles -Root $AssetsRoot -Kind Asset)
if (-not $AssetFiles.Count) { throw "No asset files found under $AssetsRoot" }

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$Partial = "$Archive.partial"
if (Test-Path -LiteralPath $Partial) { Remove-Item -LiteralPath $Partial -Force }
try {
    $Zip = [IO.Compression.ZipFile]::Open($Partial, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in $ProgramFiles) {
            [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($Zip, $file.Source, $file.Relative)
        }
        foreach ($file in $AssetFiles) {
            [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($Zip, $file.Source, "Assets/$($file.Relative)")
        }
    }
    finally { $Zip.Dispose() }

    if (-not $SkipStartupTest) {
        $ResolvedOutput = (Resolve-Path -LiteralPath $OutputDirectory).Path
        $Extracted = Join-Path $ResolvedOutput ([guid]::NewGuid().ToString('N'))
        try {
            [IO.Compression.ZipFile]::ExtractToDirectory($Partial, $Extracted)
            & (Join-Path $PSScriptRoot 'tests\app\test_startup.ps1') `
                -Executable (Join-Path $Extracted 'Folia.exe') `
                -WorkingDirectory $ResolvedOutput `
                -ReportDirectory (Join-Path $PSScriptRoot 'build\tests\startup-portable')
        }
        finally {
            if (Test-Path -LiteralPath $Extracted) {
                $ResolvedExtraction = (Resolve-Path -LiteralPath $Extracted).Path
                if ((Split-Path -Parent $ResolvedExtraction) -eq $ResolvedOutput) {
                    Remove-Item -LiteralPath $ResolvedExtraction -Recurse -Force
                }
            }
        }
    }
    Move-Item -LiteralPath $Partial -Destination $Archive -Force
}
finally {
    if (Test-Path -LiteralPath $Partial) { Remove-Item -LiteralPath $Partial -Force }
}

Write-Host "Folia portable ZIP: $Archive"
