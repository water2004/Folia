[CmdletBinding()]
param()

# Real per-user Windows Installer integration test. Uses a unique product,
# component identities, registry namespace and build/ directory; never installs
# or uninstalls Folia. Requires the same dotnet/WiX SDK as build_msi.ps1.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runId = [guid]::NewGuid().ToString('N')
$work = Join-Path $root "build/tests/theme-upgrade/$runId"
$installed = Join-Path $work 'installed'
$upgradeCode = [guid]::NewGuid().ToString()
$cleanupGuid = [guid]::NewGuid().ToString()
$componentGuids = @{}
$products = @()

function Assert([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}

function Run-Msi([string[]]$Arguments, [string]$LogName) {
    $log = Join-Path $work $LogName
    $process = Start-Process msiexec.exe -ArgumentList ($Arguments + @('/qn', '/norestart', '/l*v', "`"$log`"")) `
        -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(60000)) { throw "Installer test timed out; see $log" }
    $process.Refresh()
    Assert ($process.ExitCode -in @(0, 3010)) "Installer exit $($process.ExitCode); see $log"
}

function Build-Package([string]$Name, [bool]$OldSchema) {
    $directory = Join-Path $work $Name
    $assets = Join-Path $directory 'assets'
    $program = Join-Path $directory 'program'
    New-Item -ItemType Directory -Force -Path $program, "$assets/themes/custom" | Out-Null
    Set-Content -LiteralPath "$program/fixture.txt" -Value 'isolated installer test'
    foreach ($theme in Get-ChildItem (Join-Path $root 'src/app-winui/Assets/themes') -Filter '*.json' -File) {
        if ($OldSchema) {
            $profile = Get-Content -LiteralPath $theme.FullName -Raw | ConvertFrom-Json
            $profile.schemaVersion = 1
            $profile.PSObject.Properties.Remove('syntaxMarkers')
            $profile | ConvertTo-Json -Depth 30 | Set-Content -LiteralPath "$assets/themes/$($theme.Name)" -Encoding UTF8
        } else {
            Copy-Item -LiteralPath $theme.FullName -Destination "$assets/themes/$($theme.Name)"
        }
    }
    Set-Content -LiteralPath "$assets/settings.json" -Value 'must not be packaged'
    Set-Content -LiteralPath "$assets/themes/custom/reader.json" -Value 'must not be packaged'
    $payload = Join-Path $directory 'Payload.wxs'
    & (Join-Path $root 'tools/Generate-WixPayload.ps1') -ProgramRoot $program -AssetsRoot $assets -OutputPath $payload
    [xml]$xml = Get-Content -LiteralPath $payload -Raw
    $ns = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
    $ns.AddNamespace('w', 'http://wixtoolset.org/schemas/v4/wxs')
    Assert ($xml.SelectNodes('//w:File', $ns).Count -eq 8) 'Only seven built-in themes and the test program may be packaged'
    Assert ($xml.SelectNodes('//w:RemoveFile[@On="install"]', $ns).Count -eq 7) 'Every built-in theme needs an install replacement rule'
    foreach ($component in $xml.SelectNodes('//w:Component', $ns)) {
        $id = $component.GetAttribute('Id')
        if (-not $componentGuids.ContainsKey($id)) { $componentGuids[$id] = [guid]::NewGuid().ToString() }
        $component.SetAttribute('Guid', $componentGuids[$id])
    }
    foreach ($value in $xml.SelectNodes('//w:RegistryValue', $ns)) {
        $value.SetAttribute('Key', "Software\FoliaInstallerTests\$runId")
    }
    $xml.Save($payload)
    $product = [guid]::NewGuid().ToString()
    $script:products += $product
    @"
<Wix xmlns="http://wixtoolset.org/schemas/v4/wxs">
  <Package Name="Folia Theme Upgrade Test $runId" Manufacturer="Folia Tests" Version="1.0.0"
           ProductCode="$product" UpgradeCode="$upgradeCode" Scope="perUser" Language="1033">
    <MajorUpgrade AllowSameVersionUpgrades="yes" DowngradeErrorMessage="Test version mismatch" />
    <MediaTemplate EmbedCab="yes" />
    <StandardDirectory Id="LocalAppDataFolder">
      <Directory Id="TESTROOT" Name="FoliaThemeUpgradeTest">
        <Component Id="RootCleanup" Guid="$cleanupGuid" Bitness="always64">
          <RemoveFolder Id="RemoveProgram" Directory="INSTALLFOLDER" On="uninstall" />
          <RemoveFolder Id="RemoveAssets" Directory="ASSETFOLDER" On="uninstall" />
          <RemoveFolder Id="RemoveRoot" On="uninstall" />
          <RegistryValue Root="HKCU" Key="Software\FoliaInstallerTests\$runId" Name="Root" Type="integer" Value="1" KeyPath="yes" />
        </Component>
        <Directory Id="INSTALLFOLDER" Name="Program" />
        <Directory Id="ASSETFOLDER" Name="Assets" />
      </Directory>
    </StandardDirectory>
    <Feature Id="Test"><ComponentGroupRef Id="ProgramPayload" /><ComponentGroupRef Id="AssetPayload" /><ComponentRef Id="RootCleanup" /></Feature>
  </Package>
</Wix>
"@ | Set-Content -LiteralPath "$directory/Package.wxs" -Encoding UTF8
    @'
<Project Sdk="WixToolset.Sdk/4.0.6">
  <PropertyGroup><OutputType>Package</OutputType><Platform>x64</Platform><SuppressIces>ICE91</SuppressIces></PropertyGroup>
</Project>
'@ | Set-Content -LiteralPath "$directory/Test.wixproj" -Encoding UTF8
    & dotnet build "$directory/Test.wixproj" --nologo --configuration Release -m:4 > "$directory/build.log" 2>&1
    Assert ($LASTEXITCODE -eq 0) "WiX build failed; see $directory/build.log"
    return (Get-ChildItem -LiteralPath "$directory/bin" -Recurse -Filter '*.msi' | Select-Object -First 1).FullName
}

try {
    $old = Build-Package 'old' $true
    $new = Build-Package 'new' $false
    Run-Msi @('/i', "`"$old`"", "TESTROOT=`"$installed`"") 'install-old.log'
    $themeDirectory = Join-Path $installed 'Assets/themes'
    New-Item -ItemType Directory -Force -Path "$themeDirectory/custom" | Out-Null
    $custom = [byte[]](0, 1, 239, 187, 191, 65, 255)
    $settings = [byte[]](123, 34, 120, 34, 58, 49, 125)
    [IO.File]::WriteAllBytes("$themeDirectory/custom/reader.json", $custom)
    [IO.File]::WriteAllBytes("$installed/Assets/settings.json", $settings)
    foreach ($theme in Get-ChildItem -LiteralPath $themeDirectory -File) {
        Add-Content -LiteralPath $theme.FullName -Value 'modified old built-in'
        $theme.CreationTimeUtc = [DateTime]::UtcNow.AddDays(-1)
        $theme.LastWriteTimeUtc = [DateTime]::UtcNow
    }
    Run-Msi @('/i', "`"$new`"", "TESTROOT=`"$installed`"") 'upgrade-same-version.log'
    foreach ($phase in @('upgrade', 'repair')) {
        if ($phase -eq 'repair') {
            Add-Content -LiteralPath "$themeDirectory/light.json" -Value 'modified current built-in'
            (Get-Item -LiteralPath "$themeDirectory/light.json").CreationTimeUtc = [DateTime]::UtcNow.AddDays(-1)
            Run-Msi @('/i', "`"$new`"", 'REINSTALL=ALL', 'REINSTALLMODE=omus', "TESTROOT=`"$installed`"") 'repair.log'
        }
        foreach ($theme in Get-ChildItem (Join-Path $root 'src/app-winui/Assets/themes') -Filter '*.json' -File) {
            Assert ((Get-FileHash $theme.FullName).Hash -eq (Get-FileHash "$themeDirectory/$($theme.Name)").Hash) "$phase did not replace $($theme.Name)"
        }
        Assert ([Convert]::ToBase64String([IO.File]::ReadAllBytes("$themeDirectory/custom/reader.json")) -ceq [Convert]::ToBase64String($custom)) "$phase changed a custom theme"
        Assert ([Convert]::ToBase64String([IO.File]::ReadAllBytes("$installed/Assets/settings.json")) -ceq [Convert]::ToBase64String($settings)) "$phase changed user settings"
    }
    Write-Host 'PASS: same-version upgrade and repair replace all built-in themes; custom theme and settings bytes are unchanged.'
} finally {
    # Remove only products created by this run; keep logs/fixtures under build.
    foreach ($product in $products) {
        $process = Start-Process msiexec.exe -ArgumentList @('/x', "{$product}", '/qn', '/norestart') -WindowStyle Hidden -PassThru
        if (-not $process.WaitForExit(60000)) { throw "Test product cleanup timed out: $product" }
        $process.Refresh()
        Assert ($process.ExitCode -in @(0, 1605, 3010)) "Test product cleanup failed: $product ($($process.ExitCode))"
    }
}
