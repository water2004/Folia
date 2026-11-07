[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build/installer/native'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $installation) { throw 'Visual C++ build tools not found.' }
$devcmd = Join-Path ($installation | Select-Object -First 1) 'Common7/Tools/VsDevCmd.bat'
$source = Join-Path $root 'installer/custom_actions.cpp'
$command = 'call "' + $devcmd + '" -arch=x64 -host_arch=x64 >nul && cl /nologo /LD /MT /O2 /EHsc /std:c++latest /utf-8 "' + $source + '" /link /OUT:FoliaInstallerActions.dll msi.lib advapi32.lib'
Push-Location $output
try {
    & $env:ComSpec /d /s /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Installer custom action build failed.' }
} finally { Pop-Location }
