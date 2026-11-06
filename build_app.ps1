[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64')]
    [string]$Platform = 'x64',

    [string]$AssetsDirectory,

    [switch]$SelfContained,

    [switch]$SkipAssetsDeployment,

    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Project = Join-Path $Root 'src\app-winui\Folia.vcxproj'
$VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

if (-not (Test-Path -LiteralPath $Project -PathType Leaf)) {
    throw "Missing WinUI project: $Project"
}
if (-not (Test-Path -LiteralPath $VsWhere -PathType Leaf)) {
    throw "Visual Studio locator was not found: $VsWhere"
}

$Installation = & $VsWhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $Installation) {
    throw 'A Visual Studio installation with MSBuild was not found.'
}

$MSBuild = Join-Path ($Installation | Select-Object -First 1) 'MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $MSBuild -PathType Leaf)) {
    throw "MSBuild was not found: $MSBuild"
}
$VsDevCmd = Join-Path ($Installation | Select-Object -First 1) 'Common7\Tools\VsDevCmd.bat'
if (-not (Test-Path -LiteralPath $VsDevCmd -PathType Leaf)) {
    throw "Visual C++ environment script was not found: $VsDevCmd"
}

$NativeBuildDirectory = if ($Configuration -eq 'Debug') {
    Join-Path $Root 'build\core'
} else {
    Join-Path $Root 'build\core-release'
}

$ConfigureCommand = 'call "' + $VsDevCmd + '" -arch=x64 -host_arch=x64 && cmake -S "' +
    $Root + '" -B "' + $NativeBuildDirectory + '" -G Ninja -DCMAKE_BUILD_TYPE=' +
    $Configuration + ' -DFOLIA_BUILD_MERMAID=ON'
& $env:ComSpec /d /s /c $ConfigureCommand
if ($LASTEXITCODE -ne 0) {
    throw "Native renderer CMake configure failed with exit code $LASTEXITCODE"
}

$NativeBuildCommand = 'call "' + $VsDevCmd + '" -arch=x64 -host_arch=x64 && cmake --build "' +
    $NativeBuildDirectory + '" --parallel 4 --target FoliaNativeMath FoliaNativeMermaid'
& $env:ComSpec /d /s /c $NativeBuildCommand
if ($LASTEXITCODE -ne 0) {
    throw "Native renderer build failed with exit code $LASTEXITCODE"
}

$Properties = @(
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    '/p:CL_MPCount=4'
)
if ($AssetsDirectory) {
    # The value is emitted into a C++ wide string literal. Forward slashes are
    # accepted by Windows paths and avoid accidental C++ escape sequences.
    $CompilerAssetsDirectory = $AssetsDirectory.Replace('\', '/')
    $Properties += "/p:FoliaAssetsDirectory=$CompilerAssetsDirectory"
}
if ($SelfContained) {
    $Properties += '/p:FoliaSelfContained=true'
}
if ($SkipAssetsDeployment) {
    $Properties += '/p:FoliaDeployAssets=false'
}

if ($Clean) {
    & $MSBuild $Project /nologo /m:4 /v:minimal /t:Clean @Properties
    if ($LASTEXITCODE -ne 0) {
        throw "Folia clean failed with exit code $LASTEXITCODE"
    }
}

& $MSBuild $Project /nologo /m:4 /v:minimal /t:Build @Properties
if ($LASTEXITCODE -ne 0) {
    throw "Folia build failed with exit code $LASTEXITCODE"
}

$Output = Join-Path $Root "build\app-winui\bin\$Platform\$Configuration"
Write-Host "Folia $Configuration build: $Output"
