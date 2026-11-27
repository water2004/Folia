param(
    [switch]$ForceNuGetDownload
)

$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$PackagesConfig = Join-Path $Root 'src\app-winui\packages.config'
$PackagesDirectory = Join-Path $Root 'src\app-winui\packages'
$SvgNormalizerDirectory = Join-Path $Root 'src\platform\windows\media\svg\usvg-normalizer'
$SvgNormalizerManifest = Join-Path $SvgNormalizerDirectory 'Cargo.toml'
$SvgNormalizerTarget = Join-Path $Root 'build\dependencies\usvg-normalizer'
$SvgNormalizerOutput = Join-Path $SvgNormalizerTarget 'release\folia_svg_normalizer.dll'
$IcuVersion = '78.3'
$IcuDirectory = Join-Path $Root "build\dependencies\icu-$IcuVersion"
$IcuRoot = Join-Path $IcuDirectory 'root'
$IcuArchive = Join-Path $IcuDirectory "icu4c-$IcuVersion-Win64-MSVC2022.zip"
$IcuHeader = Join-Path $IcuRoot 'include\unicode\ucsdet.h'
$IcuUrl = "https://github.com/unicode-org/icu/releases/download/release-$IcuVersion/icu4c-$IcuVersion-Win64-MSVC2022.zip"
$IcuSha512 = '32c7b8c3581b0eed51e13bfd1c723122c5ae154fe5662000f51fa68d8ff8a633802804944963e96c79f839a360cba413ce7c6144e422da707add1b4290935889'
$ToolDirectory = Join-Path $Root '.tools'
$NuGetPath = Join-Path $ToolDirectory 'nuget.exe'
$NuGetUrl = 'https://dist.nuget.org/win-x86-commandline/latest/nuget.exe'

if (-not (Test-Path -LiteralPath $PackagesConfig)) {
    throw "Missing NuGet manifest: $PackagesConfig"
}

New-Item -ItemType Directory -Force -Path $ToolDirectory | Out-Null

if ($ForceNuGetDownload -or -not (Test-Path -LiteralPath $NuGetPath)) {
    Write-Host "Downloading NuGet command-line client..."
    Invoke-WebRequest -Uri $NuGetUrl -OutFile $NuGetPath
}

New-Item -ItemType Directory -Force -Path $PackagesDirectory | Out-Null

Write-Host "Restoring WinUI dependencies to $PackagesDirectory..."
& $NuGetPath restore $PackagesConfig `
    -PackagesDirectory $PackagesDirectory `
    -NonInteractive `
    -Verbosity normal

if ($LASTEXITCODE -ne 0) {
    throw "NuGet restore failed with exit code $LASTEXITCODE"
}

Write-Host 'NuGet dependencies restored.'

if (-not (Test-Path -LiteralPath $IcuHeader)) {
    New-Item -ItemType Directory -Force -Path $IcuDirectory | Out-Null
    if (-not (Test-Path -LiteralPath $IcuArchive)) {
        Write-Host "Downloading ICU4C $IcuVersion..."
        $IcuPartial = "$IcuArchive.partial"
        Invoke-WebRequest -Uri $IcuUrl -OutFile $IcuPartial
        Move-Item -LiteralPath $IcuPartial -Destination $IcuArchive -Force
    }
    $ActualIcuSha512 = (Get-FileHash -LiteralPath $IcuArchive -Algorithm SHA512).Hash.ToLowerInvariant()
    if ($ActualIcuSha512 -ne $IcuSha512) {
        throw "ICU4C archive checksum mismatch. Expected $IcuSha512, got $ActualIcuSha512"
    }
    Write-Host "Extracting ICU4C $IcuVersion..."
    Expand-Archive -LiteralPath $IcuArchive -DestinationPath $IcuRoot -Force
}

if (-not (Test-Path -LiteralPath $IcuHeader)) {
    throw "ICU4C headers were not produced: $IcuHeader"
}

Write-Host "ICU4C $IcuVersion is available at $IcuRoot."

if (-not (Get-Command cargo -ErrorAction SilentlyContinue)) {
    throw 'Rust cargo is required to build the native SVG normalizer.'
}

Write-Host 'Building the native SVG normalizer...'
$PreviousEncodedFlags = $env:CARGO_ENCODED_RUSTFLAGS
try {
    # Cargo's unit separator keeps each remap intact when a profile or checkout contains spaces.
    $separator = [char]31
    $flags = if ($PreviousEncodedFlags) { $PreviousEncodedFlags.Split($separator) }
             elseif ($env:RUSTFLAGS) { $env:RUSTFLAGS.Trim() -split '\s+' }
    $cargoRoot = if ($env:CARGO_HOME) { $env:CARGO_HOME } else { Join-Path $env:USERPROFILE '.cargo' }
    $rustSysroot = & rustc --print sysroot
    if ($LASTEXITCODE -ne 0) { throw 'Could not locate the active Rust toolchain.' }
    $env:CARGO_ENCODED_RUSTFLAGS = (@($flags | Where-Object { $_ }) + @(
        '-C', 'target-feature=+crt-static', "--remap-path-prefix=$Root=.",
        "--remap-path-prefix=$([IO.Path]::GetFullPath($cargoRoot))=cargo",
        "--remap-path-prefix=$rustSysroot=rust",
        '-C', 'link-arg=/PDBALTPATH:folia_svg_normalizer.pdb'
    )) -join $separator
    & cargo build --jobs 4 --locked --release --manifest-path $SvgNormalizerManifest --target-dir $SvgNormalizerTarget
    if ($LASTEXITCODE -ne 0) {
        throw "SVG normalizer build failed with exit code $LASTEXITCODE"
    }
}
finally {
    $env:CARGO_ENCODED_RUSTFLAGS = $PreviousEncodedFlags
}

if (-not (Test-Path -LiteralPath $SvgNormalizerOutput)) {
    throw "SVG normalizer output was not produced: $SvgNormalizerOutput"
}

Write-Host "Native SVG normalizer built at $SvgNormalizerOutput."
