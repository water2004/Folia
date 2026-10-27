[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$EditOnly,
    [switch]$Benchmark,
    [string]$Document
)

$ErrorActionPreference = 'Stop'

function Find-RepositoryRoot([string]$StartPath) {
    $directory = Get-Item -LiteralPath $StartPath
    while ($null -ne $directory) {
        if ((Test-Path -LiteralPath (Join-Path $directory.FullName 'build_app.ps1') -PathType Leaf) -and
            (Test-Path -LiteralPath (Join-Path $directory.FullName 'src\app-winui\Folia.vcxproj') -PathType Leaf)) {
            return $directory.FullName
        }
        $directory = $directory.Parent
    }
    throw "Could not locate repository root above $StartPath"
}

function Quote-CmdArgument([string]$Value) {
    return '"' + $Value.Replace('"', '""') + '"'
}

function Require-File([string]$Path, [string]$Description) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "Missing $Description`: $Path"
    }
}

$root = Find-RepositoryRoot $PSScriptRoot
$appSource = Join-Path $root 'src\app-winui'
$platformSource = Join-Path $root 'src\platform'
$objectDirectory = Join-Path $root "build\app-winui\obj\x64\$Configuration"
$applicationDirectory = Join-Path $root "build\app-winui\bin\x64\$Configuration"
$scratchDirectory = Join-Path $root "build\math-port\layout-probe\$Configuration"
$probeSource = Join-Path $PSScriptRoot 'winui-math-layout-probe.cpp'
$runtimeSource = Join-Path $root 'tests\support\windows_console_runtime.cpp'
$linkTlog = Join-Path $objectDirectory 'Folia.tlog\link.command.1.tlog'
$probeExecutable = Join-Path $applicationDirectory 'FoliaMathLayoutProbe.exe'
$nativeRuntime = Join-Path $root "build\core$(if ($Configuration -eq 'Release') { '-release' })\FoliaNativeMath.dll"
$nativeImportLibrary = [System.IO.Path]::ChangeExtension($nativeRuntime, '.lib')
$compileTlog = Join-Path $objectDirectory 'Folia.tlog\CL.command.1.tlog'

Require-File $probeSource 'layout probe source'
Require-File $runtimeSource 'console test runtime source'
Require-File $linkTlog 'WinUI linker dependency record'
Require-File $compileTlog 'WinUI compiler command record'
Require-File (Join-Path $objectDirectory 'pch.pch') 'WinUI precompiled header'
Require-File (Join-Path $objectDirectory 'pch.obj') 'WinUI PCH object'
Require-File $nativeImportLibrary 'native math import library'
Require-File $nativeRuntime 'native math runtime DLL'
Require-File (Join-Path $applicationDirectory 'FoliaNativeMath.dll') 'deployed native math DLL'

$requiredObjects = @(
    'EditorSession.obj',
    'EditorSurfaceRenderer.Math.obj',
    'EditorDocumentPreparationPass.obj',
    'EditorDocumentBlockPreparer.obj',
    'EditorRenderResources.obj',
    'NativeMathSession.obj',
    'MathJaxRenderer.obj',
    'MathSvgStore.obj',
    'SvgNormalizer.obj',
    'MathJaxSvgParser.obj'
)
foreach ($objectName in $requiredObjects) {
    Require-File (Join-Path $objectDirectory $objectName) "layout preparation object $objectName"
}

$allLinkRecords = @(Get-Content -LiteralPath $linkTlog)
$lastRecord = -1
for ($index = 0; $index -lt $allLinkRecords.Count; $index++) {
    if ($allLinkRecords[$index].StartsWith('^') -and $allLinkRecords[$index] -notmatch '__WINMD_ONLY__') { $lastRecord = $index }
}
if ($lastRecord -lt 0) { throw 'No executable linker record found; run build_app.ps1 first.' }
$endRecord = $lastRecord + 1
while ($endRecord -lt $allLinkRecords.Count -and -not $allLinkRecords[$endRecord].StartsWith('^')) { ++$endRecord }
$records = @($allLinkRecords[$lastRecord..($endRecord - 1)])
if ($records.Count -lt 2 -or -not $records[0].StartsWith('^')) {
    throw "Could not parse the WinUI linker dependency record: $linkTlog"
}
$appObjects = @((@($records[0].Substring(1).Split('|')) + @($records | Where-Object { $_ -match '(?i)\.obj"?$' })) |
    Where-Object { $_.EndsWith('.obj', [StringComparison]::OrdinalIgnoreCase) } |
    ForEach-Object { $_.Trim('"') } |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Sort-Object -Unique)
if ($appObjects.Count -eq 0) {
    throw 'The WinUI linker record did not contain usable object files.'
}

$libraryTokens = [regex]::Matches($records[1], '(?i)(?:"([^"]+\.lib)"|([^\s"]+\.lib))')
$libraries = @($libraryTokens | ForEach-Object {
    if ($_.Groups[1].Success) { $_.Groups[1].Value } else { $_.Groups[2].Value }
} | Select-Object -Unique)
$libraryPaths = @([regex]::Matches($records[1], '(?:^|\s)(/LIBPATH:"[^"]+"|/LIBPATH:[^\s]+)') |
    ForEach-Object { $_.Value.Trim() } | Select-Object -Unique)

$compileRecords = Get-Content -LiteralPath $compileTlog
$baselineCommand = $null
for ($index = 0; $index + 1 -lt $compileRecords.Count; $index++) {
    if ($compileRecords[$index] -match 'EDITOR\\RENDERING\\EDITORSURFACERENDERER\.MATH\.CPP$') {
        $baselineCommand = $compileRecords[$index + 1]
        break
    }
}
if (-not $baselineCommand) {
    throw 'Could not find the current EditorSurfaceRenderer.Math.cpp compile command.'
}
$vsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
Require-File $vsWhere 'Visual Studio locator'
$installation = & $vsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $installation) {
    throw 'Visual Studio C++ build tools were not found.'
}
$compilerReads = Get-Content -LiteralPath (Join-Path $objectDirectory 'Folia.tlog\CL.read.1.tlog') -Raw
$environmentScript = if ($compilerReads -match '\\HOSTX86\\X64\\') { 'vcvarsx86_amd64.bat' } else { 'vcvars64.bat' }
$vcvars = Join-Path ($installation | Select-Object -First 1) ('VC\Auxiliary\Build\' + $environmentScript)
Require-File $vcvars 'vcvars64.bat'

New-Item -ItemType Directory -Force -Path $scratchDirectory | Out-Null
New-Item -ItemType Directory -Force -Path $applicationDirectory | Out-Null
$probeObject = Join-Path $scratchDirectory 'winui_math_layout_probe.obj'
$runtimeObject = Join-Path $scratchDirectory 'windows_console_runtime.obj'
$compilerPdb = Join-Path $scratchDirectory 'layout-probe-compiler.pdb'
$linkerPdb = Join-Path $scratchDirectory 'FoliaMathLayoutProbe.pdb'
$probeResponse = Join-Path $scratchDirectory 'probe.rsp'
$runtimeResponse = Join-Path $scratchDirectory 'runtime.rsp'
$linkResponse = Join-Path $scratchDirectory 'link.rsp'
$stdoutPath = Join-Path $scratchDirectory 'probe.stdout.log'
$stderrPath = Join-Path $scratchDirectory 'probe.stderr.log'
$runtimeFlag = [regex]::Match($baselineCommand, '(?i)(?:^|\s)(/M[DT]d?)(?:\s|$)').Groups[1].Value
if (-not $runtimeFlag) { throw 'Could not read the application CRT compile option.' }

$moduleReferences = [System.Collections.Generic.List[string]]::new()
foreach ($moduleFile in Get-ChildItem -LiteralPath $objectDirectory -Filter '*.module.json' -File -Recurse) {
    $module = Get-Content -LiteralPath $moduleFile.FullName -Raw | ConvertFrom-Json
    foreach ($rule in $module.rules) {
        foreach ($provided in $rule.provides) {
            $ifcPath = @($rule.outputs | Where-Object { $_.EndsWith('.ifc', [StringComparison]::OrdinalIgnoreCase) } |
                Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1)
            if ($ifcPath.Count -gt 0) {
                $moduleReferences.Add(('/reference "{0}={1}"' -f $provided.'logical-name', $ifcPath[0]))
            }
        }
    }
}
if ($moduleReferences.Count -eq 0) {
    throw "No C++ module IFC files were found under $objectDirectory"
}

# Reuse all PCH-sensitive compiler switches from the app, including the
# host compiler's debug/runtime/module settings and its matching PCH PDB.
$probeCommand = $baselineCommand -replace '/reference\s+"[^"]+"', ''
$probeCommand = $probeCommand -replace '/Fo"[^"]+"', ('/Fo"{0}"' -f $probeObject)
$probeCommand = $probeCommand -replace '/sourceDependencies\s+"[^"]+"', ''
$probeCommand = $probeCommand -replace '/scanDependencies\s+"[^"]+"', ''
$probeCommand = $probeCommand -replace '/TP\s+.*$', ('/TP "{0}"' -f $probeSource)
Set-Content -LiteralPath $probeResponse -Value (@($probeCommand) + $moduleReferences) -Encoding ascii

$runtimeArgs = @(
    '/nologo', '/std:c++latest', '/EHsc', $runtimeFlag, '/utf-8', '/DWIN32_LEAN_AND_MEAN',
    ('/I"{0}"' -f $appSource), ('/I"{0}"' -f $platformSource),
    ('/Fo"{0}"' -f $runtimeObject), ('/Fd"{0}"' -f $compilerPdb),
    '/c', ('"{0}"' -f $runtimeSource)
)
Set-Content -LiteralPath $runtimeResponse -Value $runtimeArgs -Encoding ascii

$linkArgs = @(
    '/NOLOGO', '/SUBSYSTEM:CONSOLE', '/INCREMENTAL:NO', '/MANIFEST:NO', '/MACHINE:X64',
    '/STACK:33554432', ('/OUT:"{0}"' -f $probeExecutable), ('/PDB:"{0}"' -f $linkerPdb)
)
if ($Configuration -eq 'Debug') { $linkArgs += '/DEBUG' } else { $linkArgs += '/LTCG' }
$linkArgs += $libraryPaths
$linkArgs += @([regex]::Matches($records[1], '(?i)/(?:NODEFAULTLIB|DEFAULTLIB):(?:"[^"]+"|[^\s]+)') |
    ForEach-Object { $_.Value })
$linkArgs += $appObjects | ForEach-Object { '"{0}"' -f $_ }
$linkArgs += '"{0}"' -f (Join-Path $objectDirectory 'Folia.res')
$linkArgs += @(('"{0}"' -f $probeObject), ('"{0}"' -f $runtimeObject))
$linkArgs += $libraries | ForEach-Object { '"{0}"' -f $_ }
Set-Content -LiteralPath $linkResponse -Value $linkArgs -Encoding ascii

$command = 'call {0} >nul && cl @{1} && cl @{2} && link @{3}' -f `
    (Quote-CmdArgument $vcvars), (Quote-CmdArgument $probeResponse),
    (Quote-CmdArgument $runtimeResponse), (Quote-CmdArgument $linkResponse)
Write-Host 'Compiling and linking the DirectWrite/native math preparation probe against existing WinUI objects...'
& $env:ComSpec /d /s /c $command
if ($LASTEXITCODE -ne 0) {
    throw "Layout probe compilation/link failed with exit code $LASTEXITCODE"
}
Require-File $probeExecutable 'layout probe executable'

$started = Get-Date
$processArguments = @{
    FilePath = $probeExecutable
    WorkingDirectory = $applicationDirectory
    WindowStyle = 'Hidden'
    PassThru = $true
    RedirectStandardOutput = $stdoutPath
    RedirectStandardError = $stderrPath
}
if ($EditOnly) { $processArguments.ArgumentList = @('--edit-only') }
if ($Benchmark) { $processArguments.ArgumentList = @('--benchmark') }
if ($Document) {
    if (-not $Benchmark) { throw '-Document requires -Benchmark' }
    $processArguments.ArgumentList += '"' + (Resolve-Path -LiteralPath $Document).Path + '"'
}
$process = Start-Process @processArguments
$processHandle = $process.Handle
if (-not $process.WaitForExit(60000)) {
    try { $process.Kill() } catch { }
    $process.WaitForExit()
    if (Test-Path -LiteralPath $stdoutPath) {
        Write-Host 'probe stdout:'
        Get-Content -LiteralPath $stdoutPath -Raw
    }
    if (Test-Path -LiteralPath $stderrPath) {
        Write-Host 'probe stderr:'
        Get-Content -LiteralPath $stderrPath -Raw
    }
    throw 'Layout probe exceeded the 60-second deadline; only the probe process was terminated.'
}
$process.Refresh()
$elapsed = (Get-Date) - $started
Write-Host 'probe stdout:'
if (Test-Path -LiteralPath $stdoutPath) { Get-Content -LiteralPath $stdoutPath -Raw }
Write-Host 'probe stderr:'
if (Test-Path -LiteralPath $stderrPath) { Get-Content -LiteralPath $stderrPath -Raw }
Write-Host "probe exit code: $($process.ExitCode); elapsed: $([math]::Round($elapsed.TotalSeconds, 2))s"
if ($process.ExitCode -ne 0) {
    throw "Layout probe failed with exit code $($process.ExitCode)"
}
