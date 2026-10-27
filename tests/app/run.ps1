[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$TestFilter = '*',
    [string]$SourceFilter = '*',
    [switch]$Visual
)

# Run after build_app.ps1. Reuse its exact module/PCH/runtime/link settings so
# these tests exercise the production controllers, not a separate model.
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$objects = Join-Path $root "build/app-winui/obj/x64/$Configuration"
$app = Join-Path $root "build/app-winui/bin/x64/$Configuration"
$hostName = if ($Visual) { 'window' } else { 'controller' }
$scratch = Join-Path $root "build/tests/winui/$Configuration/$hostName"
New-Item -ItemType Directory -Force -Path $scratch | Out-Null
# The SVG normalizer resolves its DLL relative to the executable, not the
# working directory. Keep the test host's math pipeline identical to the app.
Copy-Item -LiteralPath (Join-Path $app 'folia_svg_normalizer.dll') -Destination $scratch -Force
# The visual host uses the real Application bootstrap. Controller tests keep
# their minimal WindowsXamlManager host, with no application XAML/PRI files.
if ($Visual) {
    foreach ($resource in Get-ChildItem -LiteralPath $app -File -Recurse | Where-Object { $_.Extension -in '.xbf', '.pri' }) {
        $relative = $resource.FullName.Substring($app.TrimEnd('\').Length + 1)
        $destination = Join-Path $scratch $relative
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
        Copy-Item -LiteralPath $resource.FullName -Destination $destination -Force
    }
}
$linkRecords = @(Get-Content (Join-Path $objects 'Folia.tlog/link.command.1.tlog'))
$start = -1
for ($i = 0; $i -lt $linkRecords.Count; ++$i) {
    # MSBuild also records the XAML metadata-only link. Its object list omits
    # generated XAML implementations and cannot link a runnable test host.
    if ($linkRecords[$i].StartsWith('^') -and $linkRecords[$i] -notmatch '__WINMD_ONLY__') { $start = $i }
}
if ($start -lt 0) { throw 'No executable linker record found; run build_app.ps1 first.' }
$end = $start + 1
while ($end -lt $linkRecords.Count -and -not $linkRecords[$end].StartsWith('^')) { ++$end }
$records = @($linkRecords[$start..($end - 1)])
$appObjects = @((@($records[0].Substring(1).Split('|')) + @($records | Where-Object { $_ -match '(?i)\.obj"?$' })) |
    ForEach-Object { $_.Trim('"') } |
    Where-Object { $_.EndsWith('.obj', [StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $_) } |
    ForEach-Object { [IO.Path]::GetFullPath($_) } |
    Sort-Object -Unique)
if ($appObjects.Count -eq 0) { throw 'No application objects found; run build_app.ps1 first.' }
if ($Visual) { $appObjects = @($appObjects | Where-Object { [IO.Path]::GetFileName($_) -ine 'App.xaml.obj' }) }
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
$sources = if ($Visual) { Join-Path $root 'tests/app/visual' } else { Join-Path $root 'tests/app' }
$entrySource = if ($Visual) { 'main' } else { 'test_document_loading' }
$selectedSources = @(Get-ChildItem $sources -Filter '*.cpp' | Where-Object { $_.BaseName -eq $entrySource -or $_.BaseName -like $SourceFilter })
if (-not @(Get-ChildItem $sources -Filter '*.cpp' | Where-Object { $_.BaseName -like $SourceFilter }).Count) { throw 'No test source files match SourceFilter.' }
foreach ($source in $selectedSources) {
    Compile $source.BaseName $source.FullName
}
Compile 'windows_console_runtime' (Join-Path $root 'tests/support/windows_console_runtime.cpp') -NoPch
$executable = Join-Path $scratch $(if ($Visual) { 'Folia.exe' } else { 'FoliaWinUITests.exe' })
$linkResponse = Join-Path $scratch 'link.rsp'
$linkFlags = if ($Configuration -eq 'Debug') { '/DEBUG' } else { '/LTCG' }
$manifestFlags = '/MANIFEST:NO'
if ($Visual) {
    # Reuse the application's merged manifest, including PerMonitorV2 and
    # runtime dependencies. A DPI-unaware visual host is bitmap-scaled by
    # Windows and cannot validate production text or layout.
    $visualManifest = Join-Path $scratch 'app.manifest'
    & mt.exe "-inputresource:$(Join-Path $app 'Folia.exe');#1" "-out:$visualManifest" -nologo
    if ($LASTEXITCODE -ne 0) { throw 'Cannot extract the production application manifest.' }
    $manifestFlags = '/MANIFEST:EMBED /MANIFESTUAC:NO /MANIFESTINPUT:"{0}"' -f $visualManifest
}
$subsystem = if ($Visual) { 'WINDOWS' } else { 'CONSOLE' }
Set-Content -LiteralPath $linkResponse -Encoding ascii -Value ((@(
    "/NOLOGO /SUBSYSTEM:$subsystem /INCREMENTAL:NO $manifestFlags /MACHINE:X64 /STACK:33554432",
    $linkFlags, ('/OUT:"{0}" /PDB:"{1}"' -f $executable, (Join-Path $scratch 'tests.pdb'))) +
    @($appObjects + $testObjects + (Join-Path $objects 'Folia.res') | ForEach-Object { '"{0}"' -f $_ }) + $libraryArgs) -join "`n")
& link.exe "@$linkResponse"
if ($LASTEXITCODE -ne 0) { throw 'WinUI test link failed.' }
if ($Visual) { $env:FOLIA_VISUAL_TEST_RESULT = Join-Path $scratch ('result-' + [guid]::NewGuid().ToString('N') + '.txt') }
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
if ($Visual -and ((-not (Test-Path -LiteralPath $env:FOLIA_VISUAL_TEST_RESULT)) -or ([IO.File]::ReadAllText($env:FOLIA_VISUAL_TEST_RESULT).Trim() -ne '0'))) {
    throw 'WinUI visual assertions did not pass.'
}
