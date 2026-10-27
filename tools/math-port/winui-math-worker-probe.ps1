[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    # Validate an existing payload without changing its self-contained or
    # Assets build properties, as required after building an MSI.
    [switch]$SkipBuild,

    [switch]$SelfContained,

    [switch]$CancelOnly,

    [switch]$IndexOnly,

    [switch]$CacheOnly
)

$ErrorActionPreference = 'Stop'
if (@([bool]$CancelOnly, [bool]$IndexOnly, [bool]$CacheOnly).Where({ $_ }).Count -gt 1) {
    throw 'Choose only one of -CancelOnly, -IndexOnly, or -CacheOnly.'
}

function Find-RepositoryRoot([string]$StartPath) {
    $directory = Get-Item -LiteralPath $StartPath
    while ($null -ne $directory) {
        $buildScript = Join-Path $directory.FullName 'build_app.ps1'
        $project = Join-Path $directory.FullName 'src\app-winui\Folia.vcxproj'
        if ((Test-Path -LiteralPath $buildScript -PathType Leaf) -and
            (Test-Path -LiteralPath $project -PathType Leaf)) {
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

function Remove-VerifiedJunction([string]$Path, [string]$FixtureRoot) {
    $separator = [IO.Path]::DirectorySeparatorChar
    $fixtureFullPath = [IO.Path]::GetFullPath($FixtureRoot).TrimEnd($separator) + $separator
    $junctionFullPath = [IO.Path]::GetFullPath($Path)
    if (-not $junctionFullPath.StartsWith($fixtureFullPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove junction outside its fixture: $junctionFullPath"
    }
    $junctionItem = Get-Item -LiteralPath $junctionFullPath -Force
    if (-not ($junctionItem.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Refusing to remove a fixture path that is not a reparse point: $junctionFullPath"
    }
    [IO.Directory]::Delete($junctionFullPath)
}

$root = Find-RepositoryRoot $PSScriptRoot
$buildApp = Join-Path $root 'build_app.ps1'
$probeSource = Join-Path $PSScriptRoot 'winui-math-worker-probe.cpp'
$runtimeSource = Join-Path $root 'tests\support\windows_console_runtime.cpp'
$appSource = Join-Path $root 'src\app-winui'
$platformSource = Join-Path $root 'src\platform'
$configurationDirectory = Join-Path $root "build\app-winui\obj\x64\$Configuration"
$generatedFilesDirectory = Join-Path $configurationDirectory 'Generated Files'
$applicationDirectory = Join-Path $root "build\app-winui\bin\x64\$Configuration"
$scratchDirectory = Join-Path $root "build\math-port\worker-probe\$Configuration"
$nativeBuildDirectory = if ($Configuration -eq 'Debug') {
    Join-Path $root 'build\core'
} else {
    Join-Path $root 'build\core-release'
}
$nativeImportLibrary = Join-Path $nativeBuildDirectory 'FoliaNativeMath.lib'
$nativeRuntime = Join-Path $nativeBuildDirectory 'FoliaNativeMath.dll'
$probeExecutable = Join-Path $applicationDirectory 'FoliaMathWorkerProbe.exe'

New-Item -ItemType Directory -Force -Path $scratchDirectory | Out-Null
if (-not $SkipBuild) {
    Write-Host "Building FoliaNativeMath and the WinUI application ($Configuration)..."
    $buildArguments = @{ Configuration = $Configuration }
    if ($SelfContained) { $buildArguments.SelfContained = $true }
    & $buildApp @buildArguments
    if (-not $?) {
        throw "build_app.ps1 failed for $Configuration"
    }
} else {
    Write-Host "Using the existing Folia application payload ($Configuration)..."
}

Require-File $probeSource 'worker probe source'
Require-File $runtimeSource 'console test runtime source'
Require-File (Join-Path $configurationDirectory 'pch.obj') 'WinUI PCH object'
Require-File (Join-Path $generatedFilesDirectory 'winrt\base.h') 'app-matched C++/WinRT base header'
Require-File (Join-Path $configurationDirectory 'MathJaxRenderer.obj') 'MathJaxRenderer object'
Require-File (Join-Path $configurationDirectory 'MathJaxRenderer.Cache.obj') 'MathJaxRenderer cache object'
Require-File (Join-Path $configurationDirectory 'MathJaxRenderer.Worker.obj') 'MathJaxRenderer worker object'
Require-File (Join-Path $configurationDirectory 'MathJaxRenderer.Batch.obj') 'MathJaxRenderer batch object'
Require-File (Join-Path $configurationDirectory 'NativeMathSession.obj') 'NativeMathSession object'
Require-File (Join-Path $configurationDirectory 'MathSvgStore.obj') 'MathSvgStore object'
Require-File (Join-Path $configurationDirectory 'SvgNormalizer.obj') 'SVG normalizer object'
Require-File (Join-Path $configurationDirectory 'MathJaxSvgParser.obj') 'MathJaxSvgParser object'
Require-File (Join-Path $configurationDirectory 'editor_priority_work_queue.ixx.obj') 'priority queue module object'
Require-File (Join-Path $configurationDirectory 'Microsoft\STL\std.ixx.obj') 'MSVC std module object'
Require-File $nativeImportLibrary 'native math import library'
Require-File $nativeRuntime 'native math runtime DLL'

$vsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
Require-File $vsWhere 'Visual Studio locator'
$installation = & $vsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $installation) {
    throw 'Visual Studio C++ build tools were not found.'
}
$vcvars64 = Join-Path ($installation | Select-Object -First 1) 'VC\Auxiliary\Build\vcvars64.bat'
Require-File $vcvars64 'vcvars64.bat'

$runtimeObject = Join-Path $scratchDirectory 'windows_console_runtime.obj'
$probeObject = Join-Path $scratchDirectory 'winui_math_worker_probe.obj'
$compilerPdb = Join-Path $scratchDirectory 'worker-probe-compiler.pdb'
$linkerPdb = Join-Path $scratchDirectory 'FoliaMathWorkerProbe.pdb'
$stdoutPath = Join-Path $scratchDirectory 'probe.stdout.log'
$stderrPath = Join-Path $scratchDirectory 'probe.stderr.log'
$runtimeFlag = if ($SelfContained) {
    if ($Configuration -eq 'Debug') { '/MTd' } else { '/MT' }
} else {
    if ($Configuration -eq 'Debug') { '/MDd' } else { '/MD' }
}
$debugFlag = if ($Configuration -eq 'Debug') { '/Zi' } else { '' }
$compileFlags = @(
    '/nologo', '/std:c++latest', '/EHsc', $runtimeFlag, $debugFlag, '/utf-8',
    ('/I' + (Quote-CmdArgument $generatedFilesDirectory)),
    '/DWIN32_LEAN_AND_MEAN', ('/I' + (Quote-CmdArgument $appSource)),
    ('/I' + (Quote-CmdArgument $platformSource))
) | Where-Object { $_ }

$compileProbe = 'cl ' + (($compileFlags + @(
    '/c', (Quote-CmdArgument $probeSource),
    ('/Fo' + (Quote-CmdArgument $probeObject)),
    ('/Fd' + (Quote-CmdArgument $compilerPdb))
)) -join ' ')
$compileRuntime = 'cl ' + (($compileFlags + @(
    '/c', (Quote-CmdArgument $runtimeSource),
    ('/Fo' + (Quote-CmdArgument $runtimeObject)),
    ('/Fd' + (Quote-CmdArgument $compilerPdb))
)) -join ' ')
$linkInputs = @(
    (Quote-CmdArgument $probeObject),
    (Quote-CmdArgument $runtimeObject),
    (Quote-CmdArgument (Join-Path $configurationDirectory 'pch.obj')),
    (Quote-CmdArgument (Join-Path $configurationDirectory 'MathJaxRenderer.obj')),
    (Quote-CmdArgument (Join-Path $configurationDirectory 'MathJaxRenderer.Cache.obj')),
    (Quote-CmdArgument (Join-Path $configurationDirectory 'MathJaxRenderer.Worker.obj')),
    (Quote-CmdArgument (Join-Path $configurationDirectory 'MathJaxRenderer.Batch.obj')),
    (Quote-CmdArgument (Join-Path $configurationDirectory 'NativeMathSession.obj')),
    (Quote-CmdArgument (Join-Path $configurationDirectory 'MathSvgStore.obj')),
    (Quote-CmdArgument (Join-Path $configurationDirectory 'SvgNormalizer.obj')),
    (Quote-CmdArgument (Join-Path $configurationDirectory 'MathJaxSvgParser.obj')),
    (Quote-CmdArgument (Join-Path $configurationDirectory 'editor_priority_work_queue.ixx.obj')),
    (Quote-CmdArgument (Join-Path $configurationDirectory 'Microsoft\STL\std.ixx.obj')),
    (Quote-CmdArgument $nativeImportLibrary)
)
$linkCommand = 'link /NOLOGO /SUBSYSTEM:CONSOLE /INCREMENTAL:NO /MANIFEST:NO /STACK:33554432 /OUT:' +
    (Quote-CmdArgument $probeExecutable) + ' /PDB:' + (Quote-CmdArgument $linkerPdb)
if ($Configuration -eq 'Debug') {
    $linkCommand += ' /DEBUG'
} else {
    $linkCommand += ' /LTCG'
}
if ($SelfContained) {
    # Match the self-contained application's CRT linkage in Folia.vcxproj.
    $staticUcrt = if ($Configuration -eq 'Debug') { 'libucrtd.lib' } else { 'libucrt.lib' }
    $dynamicUcrt = if ($Configuration -eq 'Debug') { 'ucrtd.lib' } else { 'ucrt.lib' }
    $linkCommand += " /NODEFAULTLIB:$staticUcrt /DEFAULTLIB:$dynamicUcrt"
}
$linkCommand += ' ' + ($linkInputs -join ' ') + ' windowsapp.lib runtimeobject.lib ole32.lib'

$cmdLine = 'call ' + (Quote-CmdArgument $vcvars64) + ' >nul && ' +
    $compileProbe + ' && ' + $compileRuntime + ' && ' + $linkCommand
Write-Host 'Compiling and linking the asynchronous math/cache probe...'
& $env:ComSpec /d /s /c $cmdLine
if ($LASTEXITCODE -ne 0) {
    throw "Asynchronous math/cache probe compilation/link failed with exit code $LASTEXITCODE"
}

Require-File $probeExecutable 'worker probe executable'
Require-File (Join-Path $applicationDirectory 'FoliaNativeMath.dll') 'deployed native math DLL'
$started = Get-Date
$cacheFixture = $null
$junctionPaths = @()
if ($CacheOnly) {
    $separator = [IO.Path]::DirectorySeparatorChar
    $scratchResolvedPath = (Resolve-Path -LiteralPath $scratchDirectory).Path
    $scratchFullPath = [IO.Path]::GetFullPath($scratchResolvedPath).TrimEnd($separator) + $separator
    $cacheFixture = Join-Path $scratchFullPath ("cache-reclaim-" + [guid]::NewGuid().ToString('N'))
    $fixtureFullPath = [IO.Path]::GetFullPath($cacheFixture)
    if (-not $fixtureFullPath.StartsWith($scratchFullPath, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Cache fixture path escaped the probe scratch directory: $fixtureFullPath"
    }
    New-Item -ItemType Directory -Path $fixtureFullPath | Out-Null

    $cacheFixtureDirectory = Join-Path $fixtureFullPath 'FoliaMath'
    $junctionTarget = Join-Path $fixtureFullPath 'junction-target'
    $reparseRoot = Join-Path $fixtureFullPath 'reparse-root'
    New-Item -ItemType Directory -Path $cacheFixtureDirectory | Out-Null
    New-Item -ItemType Directory -Path $junctionTarget | Out-Null
    New-Item -ItemType Directory -Path $reparseRoot | Out-Null
    $directJunction = Join-Path $cacheFixtureDirectory '{74a13d90-4e9d-4f16-8207-97e949a6f001}.svgstore'
    $rootJunction = Join-Path $reparseRoot 'FoliaMath'
    try {
        New-Item -ItemType Junction -Path $directJunction -Target $junctionTarget | Out-Null
        $junctionPaths += $directJunction
        New-Item -ItemType Junction -Path $rootJunction -Target $junctionTarget | Out-Null
        $junctionPaths += $rootJunction
        $targetCache = Join-Path $junctionTarget '{74a13d90-4e9d-4f16-8207-97e949a6f002}.svgstore'
        [IO.File]::WriteAllText($targetCache, '')
        [IO.File]::WriteAllText((Join-Path $junctionTarget 'owned-target.txt'), 'keep this target')
        [IO.File]::WriteAllText((Join-Path $fixtureFullPath 'junctions.ready'), 'ready')
        Write-Host 'cache reparse fixtures: directory junctions created inside isolated fixture'
    }
    catch {
        Write-Host "cache reparse fixtures skipped: $($_.Exception.Message)"
        foreach ($junctionPath in $junctionPaths) {
            try { Remove-VerifiedJunction $junctionPath $fixtureFullPath } catch { }
        }
        $junctionPaths = @()
    }
}

try {
    $startArguments = @{
        FilePath = $probeExecutable
        WorkingDirectory = $applicationDirectory
        WindowStyle = 'Hidden'
        PassThru = $true
        RedirectStandardOutput = $stdoutPath
        RedirectStandardError = $stderrPath
    }
    if ($CancelOnly) { $startArguments.ArgumentList = @('--cancel-only') }
    if ($IndexOnly) { $startArguments.ArgumentList = @('--index-only') }
    if ($CacheOnly) {
        $startArguments.ArgumentList = @('--cache-only', (Quote-CmdArgument $fixtureFullPath))
    }
    $process = Start-Process @startArguments
    # Retain the process handle before waiting, including on Windows PowerShell 5.1.
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
        Write-Host 'probe exit code: timeout (probe process terminated)'
        throw 'Asynchronous math/cache probe exceeded the 60-second deadline; only the probe process was terminated.'
    }
    $process.Refresh()
    $exitCode = $process.ExitCode
    $elapsed = (Get-Date) - $started
    Write-Host 'probe stdout:'
    if (Test-Path -LiteralPath $stdoutPath) { Get-Content -LiteralPath $stdoutPath -Raw }
    Write-Host 'probe stderr:'
    if (Test-Path -LiteralPath $stderrPath) { Get-Content -LiteralPath $stderrPath -Raw }
    Write-Host "probe exit code: $exitCode; elapsed: $([math]::Round($elapsed.TotalSeconds, 2))s"
    if ($exitCode -ne 0) {
        throw "Asynchronous math/cache probe failed with exit code $exitCode"
    }
}
finally {
    if ($cacheFixture -and (Test-Path -LiteralPath $cacheFixture)) {
        $scratchResolvedPath = (Resolve-Path -LiteralPath $scratchDirectory).Path
        $resolvedScratch = [IO.Path]::GetFullPath($scratchResolvedPath).TrimEnd($separator) + $separator
        $resolvedFixture = (Resolve-Path -LiteralPath $cacheFixture).Path
        $resolvedFixtureFullPath = [IO.Path]::GetFullPath($resolvedFixture)
        if (-not $resolvedFixtureFullPath.StartsWith($resolvedScratch, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove cache fixture outside probe scratch directory: $resolvedFixtureFullPath"
        }
        $resolvedFixturePrefix = $resolvedFixtureFullPath.TrimEnd($separator) + $separator
        foreach ($junctionPath in $junctionPaths) {
            $junctionFullPath = [IO.Path]::GetFullPath($junctionPath)
            if (-not $junctionFullPath.StartsWith($resolvedFixturePrefix, [StringComparison]::OrdinalIgnoreCase)) {
                throw "Refusing to remove junction outside cache fixture: $junctionFullPath"
            }
            if (Test-Path -LiteralPath $junctionFullPath) {
                Remove-VerifiedJunction $junctionFullPath $resolvedFixtureFullPath
            }
        }
        Get-ChildItem -LiteralPath $resolvedFixtureFullPath -Force -Recurse | ForEach-Object {
            $_.Attributes = $_.Attributes -band (-bnot [IO.FileAttributes]::ReadOnly)
        }
        Remove-Item -LiteralPath $resolvedFixtureFullPath -Recurse -Force
    }
}
