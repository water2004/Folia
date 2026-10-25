[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$TestFilter = '*'
)

# Run after build_app.ps1. Reuse its exact module/PCH/runtime/link settings so
# these tests exercise the production controllers, not a separate model.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$objects = Join-Path $root "build/app-winui/obj/x64/$Configuration"
$app = Join-Path $root "build/app-winui/bin/x64/$Configuration"
$scratch = Join-Path $root "build/tests/winui/$Configuration"
New-Item -ItemType Directory -Force -Path $scratch | Out-Null
# The SVG normalizer resolves its DLL relative to the executable, not the
# working directory. Keep the test host's math pipeline identical to the app.
Copy-Item -LiteralPath (Join-Path $app 'folia_svg_normalizer.dll') -Destination $scratch -Force
$linkRecords = @(Get-Content (Join-Path $objects 'Folia.tlog/link.command.1.tlog'))
$start = 0
for ($i = 0; $i -lt $linkRecords.Count; ++$i) {
    if ($linkRecords[$i].StartsWith('^')) { $start = $i }
}
$records = @($linkRecords[$start..($linkRecords.Count - 1)])
$appObjects = @((@($records[0].Substring(1).Split('|')) + @($records | Where-Object { $_ -match '(?i)\.obj"?$' })) |
    ForEach-Object { $_.Trim('"') } |
    Where-Object { $_.EndsWith('.obj', [StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $_) } |
    ForEach-Object { [IO.Path]::GetFullPath($_) } |
    Sort-Object -Unique)
if ($appObjects.Count -eq 0) { throw 'No application objects found; run build_app.ps1 first.' }
$libraryCommand = $records[1] -replace '(?i)/IMPLIB:"[^"]+"', ''
$libraryArgs = @([regex]::Matches($libraryCommand, '(?i)(?:"[^"]+\.lib"|[^\s"]+\.lib|/LIBPATH:"[^"]+"|/(?:NODEFAULTLIB|DEFAULTLIB):[^\s]+)') |
    ForEach-Object { $_.Value } | Select-Object -Unique)
$compileRecords = @(Get-Content (Join-Path $objects 'Folia.tlog/CL.command.1.tlog'))
$baseline = $null
for ($i = 0; $i + 1 -lt $compileRecords.Count; ++$i) {
    if ($compileRecords[$i] -match 'EDITORDOCUMENTCONTROLLER\.CPP$') { $baseline = $compileRecords[$i + 1]; break }
}
if (-not $baseline) { throw 'No controller compile command found; run build_app.ps1 first.' }
$references = @()
foreach ($file in Get-ChildItem -LiteralPath $objects -Filter '*.module.json' -File -Recurse) {
    $module = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
    foreach ($rule in $module.rules) {
        foreach ($provided in $rule.provides) {
            $ifc = @($rule.outputs | Where-Object { $_.EndsWith('.ifc') -and (Test-Path -LiteralPath $_) } | Select-Object -First 1)
            if ($ifc.Count) { $references += '/reference "{0}={1}"' -f $provided.'logical-name', $ifc[0] }
        }
    }
}
if (-not $references.Count) { throw 'No application module references found.' }
$flags = $baseline -replace '/reference\s+"[^"]+"', ''
$flags = $flags -replace '/(?:sourceDependencies|scanDependencies)\s+"[^"]+"', ''
$flags = $flags -replace '/Fo"[^"]+"', ''
$flags = $flags -replace '/TP\s+.*$', ''
$flags += ' /I"{0}"' -f (Join-Path $root 'tests')

function Compile([string]$Name, [string]$Source, [string]$Extra = '', [switch]$NoPch) {
    $localFlags = $flags
    if ($NoPch) {
        $localFlags = $localFlags -replace '/(?:Yu|Fp|FI)"[^"]*"', ''
        $localFlags += ' /Y-'
    }
    $response = Join-Path $scratch "$Name.rsp"
    $object = Join-Path $scratch "$Name.obj"
    Set-Content -LiteralPath $response -Encoding ascii -Value ((@($localFlags) + $references + @(
        $Extra, ('/Fo"{0}" /TP "{1}"' -f $object, $Source))) -join "`n")
    & cl.exe "@$response"
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed: $Name" }
    $script:testObjects += $object
}

$vsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$installation = & $vsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual Studio C++ tools not found.' }
$reads = Get-Content (Join-Path $objects 'Folia.tlog/CL.read.1.tlog') -Raw
$vcvarsName = if ($reads -match '\\HOSTX86\\X64\\') { 'vcvarsx86_amd64.bat' } else { 'vcvars64.bat' }
$vcvars = Join-Path ($installation | Select-Object -First 1) "VC/Auxiliary/Build/$vcvarsName"
# Import the compiler environment without rewriting any user shell settings.
& $env:ComSpec /d /s /c "`"$vcvars`" >nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
}
if ($LASTEXITCODE -ne 0) { throw 'Visual Studio environment initialization failed.' }
$testObjects = @()
$boostIfc = Join-Path $scratch 'boost.ut.ifc'
Compile 'boost.ut' (Join-Path $root 'third_party/ut/include/boost/ut.cppm') "/interface /ifcOutput `"$boostIfc`"" -NoPch
$references += '/reference "boost.ut={0}"' -f $boostIfc
foreach ($source in Get-ChildItem (Join-Path $root 'tests/app') -Filter '*.cpp') {
    Compile $source.BaseName $source.FullName
}
Compile 'windows_console_runtime' (Join-Path $root 'tools/test-support/windows_console_runtime.cpp') -NoPch
$executable = Join-Path $scratch 'FoliaWinUITests.exe'
$linkResponse = Join-Path $scratch 'link.rsp'
$linkFlags = if ($Configuration -eq 'Debug') { '/DEBUG' } else { '/LTCG' }
Set-Content -LiteralPath $linkResponse -Encoding ascii -Value ((@(
    '/NOLOGO /SUBSYSTEM:CONSOLE /INCREMENTAL:NO /MANIFEST:NO /MACHINE:X64 /STACK:33554432',
    $linkFlags, ('/OUT:"{0}" /PDB:"{1}"' -f $executable, (Join-Path $scratch 'tests.pdb'))) +
    @($appObjects + $testObjects + (Join-Path $objects 'Folia.res') | ForEach-Object { '"{0}"' -f $_ }) + $libraryArgs) -join "`n")
& link.exe "@$linkResponse"
if ($LASTEXITCODE -ne 0) { throw 'WinUI test link failed.' }
$process = Start-Process -FilePath $executable -ArgumentList "`"$TestFilter`"" -WorkingDirectory $app -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput (Join-Path $scratch 'stdout.log') -RedirectStandardError (Join-Path $scratch 'stderr.log')
$null = $process.Handle
if (-not $process.WaitForExit(60000)) {
    $process.Kill()
    $process.WaitForExit()
    throw 'WinUI tests exceeded 60 seconds; only the test process was stopped.'
}
$process.Refresh()
Get-Content (Join-Path $scratch 'stdout.log')
Get-Content (Join-Path $scratch 'stderr.log')
if ($process.ExitCode -ne 0) { throw "WinUI tests failed: $($process.ExitCode)" }
