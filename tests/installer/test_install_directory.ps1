[CmdletBinding()]
param()

# Exercise the production WiX authoring with unique product/component identities
# and registry keys. This fixture never installs or removes the user's Folia.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$runId = [guid]::NewGuid().ToString('N')
$work = Join-Path $root "build/tests/install-directory/$runId"
$program = Join-Path $work 'program'
$assets = Join-Path $work 'assets'
$registryKey = "Software\FoliaInstallerTests\$runId"
$upgradeCode = [guid]::NewGuid().ToString()
$componentGuids = @{}
$products = @()
$defaultDirectory = Join-Path $env:LOCALAPPDATA "FoliaInstallerTest-$runId\Program"
$customDirectory = Join-Path $work '自选目录 with spaces\Folia'
$movedDirectory = Join-Path $work '另一个目录\Folia'
$dataDirectory = Join-Path $env:LOCALAPPDATA "FoliaInstallerDataTest-$runId"
$credentialTarget = "FoliaInstallerTests/$runId/APIKey"
$installer = New-Object -ComObject WindowsInstaller.Installer

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class FoliaInstallerCredentialTest {
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)]
    struct Credential {
        public uint Flags, Type;
        public string TargetName, Comment;
        public System.Runtime.InteropServices.ComTypes.FILETIME LastWritten;
        public uint BlobSize;
        public IntPtr Blob;
        public uint Persist, AttributeCount;
        public IntPtr Attributes;
        public string TargetAlias, UserName;
    }
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern bool CredWriteW(ref Credential credential, uint flags);
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    static extern bool CredReadW(string target, uint type, uint flags, out IntPtr credential);
    [DllImport("advapi32.dll")] static extern void CredFree(IntPtr credential);
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode)] static extern bool CredDeleteW(string target, uint type, uint flags);
    public static void Seed(string target) {
        var credential = new Credential { Type=1, TargetName=target, Persist=2, UserName="Folia installer test" };
        if (!CredWriteW(ref credential,0)) throw new System.ComponentModel.Win32Exception();
    }
    public static bool Exists(string target) {
        IntPtr credential;
        if (!CredReadW(target,1,0,out credential)) return false;
        CredFree(credential);
        return true;
    }
    public static void Clear(string target) { CredDeleteW(target,1,0); }
}
'@

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

function Build-Package([string]$Name) {
    $directory = Join-Path $work $Name
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    $payload = Join-Path $work "$Name-Payload.wxs"
    & (Join-Path $root 'tools/Generate-WixPayload.ps1') -ProgramRoot $program -AssetsRoot $assets -OutputPath $payload
    [xml]$xml = Get-Content -LiteralPath "$root/installer/Package.wxs" -Raw
    $ns = [System.Xml.XmlNamespaceManager]::new($xml.NameTable)
    $ns.AddNamespace('w', 'http://wixtoolset.org/schemas/v4/wxs')
    $package = $xml.SelectSingleNode('//w:Package', $ns)
    $product = [guid]::NewGuid().ToString()
    $script:products += $product
    $package.SetAttribute('ProductCode', $product)
    $package.SetAttribute('UpgradeCode', $upgradeCode)
    $package.SetAttribute('Name', "Folia Installer Test $runId")
    $xml.SelectSingleNode('//w:Directory[@Id="LOCALPROGRAMS"]', $ns).SetAttribute('Name', "FoliaInstallerTest-$runId")
    $xml.SelectSingleNode('//w:Directory[@Id="INSTALLFOLDER"]', $ns).SetAttribute('Name', 'Program')
    $xml.SelectSingleNode('//w:Directory[@Id="FOLIADATADIR"]', $ns).SetAttribute('Name', "FoliaInstallerDataTest-$runId")
    $xml.SelectSingleNode('//w:Property[@Id="FOLIAUSERDATADIR"]', $ns).SetAttribute('Value', $dataDirectory)
    $xml.SelectSingleNode('//w:SetProperty[@Id="FoliaRemoveCompletionCredential"]', $ns).SetAttribute('Value', $credentialTarget)
    $xml.SelectSingleNode('//w:Directory[@Id="FoliaProgramMenu"]', $ns).SetAttribute('Name', "Folia Installer Test $runId")
    $xml.SelectSingleNode('//w:Shortcut', $ns).SetAttribute('Name', "Folia Installer Test $runId")

    [xml]$payloadXml = Get-Content -LiteralPath $payload -Raw
    foreach ($document in @($xml, $payloadXml)) {
        $namespaces = [System.Xml.XmlNamespaceManager]::new($document.NameTable)
        $namespaces.AddNamespace('w', 'http://wixtoolset.org/schemas/v4/wxs')
        foreach ($component in $document.SelectNodes('//w:Component', $namespaces)) {
            $id = $component.GetAttribute('Id')
            if (-not $componentGuids.ContainsKey($id)) { $componentGuids[$id] = [guid]::NewGuid().ToString() }
            $component.SetAttribute('Guid', $componentGuids[$id])
        }
        foreach ($value in $document.SelectNodes('//w:RegistryValue | //w:RegistrySearch', $namespaces)) {
            # Preserve the original key structure inside the fixture namespace.
            $value.SetAttribute('Key', "$registryKey\$($value.GetAttribute('Key'))")
        }
    }
    $xml.Save((Join-Path $directory 'Package.wxs'))
    $payloadXml.Save($payload)
    Copy-Item -LiteralPath "$root/installer/Folia.Installer.wixproj" -Destination "$directory/Test.wixproj"
    & dotnet build "$directory/Test.wixproj" --nologo --configuration Release `
        "-p:FoliaProductVersion=1.0.0" "-p:FoliaGeneratedPayload=$payload" `
        "-p:FoliaInstallerIntermediatePath=$directory/obj/" "-p:FoliaInstallerOutputPath=$directory/bin/" `
        "-p:FoliaInstallerActionsDll=$root/build/installer/native/FoliaInstallerActions.dll" `
        "-p:FoliaLicenseRtf=$root/installer/License.rtf" "-p:FoliaProductIcon=$root/src/app-winui/Assets/branding/Folia.ico" `
        > "$directory/build.log" 2>&1
    Assert ($LASTEXITCODE -eq 0) "WiX build failed; see $directory/build.log"
    $msi = (Get-ChildItem -LiteralPath "$directory/bin" -Recurse -Filter '*.msi' | Select-Object -First 1).FullName
    $database = $installer.OpenDatabase($msi, 0)
    try {
        $query = $database.OpenView('SELECT `Value` FROM `Property` WHERE `Property`=''WIXUI_INSTALLDIR''')
        [void]$query.Execute()
        Assert ($query.Fetch().StringData(1) -eq 'INSTALLFOLDER') 'Directory UI must use the production directory property'
        [void]$query.Close()
        $query = $database.OpenView('SELECT `Dialog` FROM `Dialog` WHERE `Dialog`=''InstallDirDlg''')
        [void]$query.Execute()
        Assert ($null -ne $query.Fetch()) 'Native directory selection dialog is missing'
        [void]$query.Close()
        $query = $database.OpenView('SELECT `Property`, `Attributes` FROM `Control` WHERE `Dialog_`=''FoliaRemoveOptionsDlg'' AND `Control`=''RemoveUserData''')
        [void]$query.Execute()
        Assert ($query.Fetch().StringData(1) -eq 'FOLIAREMOVEUSERDATA') 'Full uninstall checkbox is missing'
        [void]$query.Close()
        $query = $database.OpenView('SELECT `Value` FROM `Property` WHERE `Property`=''FOLIAREMOVEUSERDATA''')
        [void]$query.Execute()
        Assert ($null -eq $query.Fetch()) 'Full uninstall must default to keeping user data'
        [void]$query.Close()
    } finally {
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($database)
    }
    return $msi
}

function Assert-Location([string]$Directory) {
    Assert (Test-Path -LiteralPath "$Directory/Folia.exe") "Program missing from $Directory"
    $saved = (Get-ItemProperty -LiteralPath "HKCU:\$registryKey\Software\water2004\Folia\Installer").InstallDirectory
    Assert ($saved.TrimEnd('\') -eq $Directory.TrimEnd('\')) 'Selected directory was not persisted'
    $command = (Get-Item -LiteralPath "HKCU:\$registryKey\Software\Classes\Folia.Markdown\shell\open\command").GetValue('')
    Assert ($command -eq ('"{0}\Folia.exe" "%1"' -f $Directory.TrimEnd('\'))) 'File association points to another directory'
}

New-Item -ItemType Directory -Force -Path $program, $assets | Out-Null
Set-Content -LiteralPath "$program/Folia.exe" -Value 'isolated install directory fixture'
. (Join-Path $root 'tools/Get-DeploymentFiles.ps1')
foreach ($asset in (Get-DeploymentFiles -Root (Join-Path $root 'src/app-winui/Assets') -Kind Asset)) {
    $target = Join-Path $assets $asset.Relative
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent $target))
    Copy-Item -LiteralPath $asset.Source -Destination $target
}
& (Join-Path $root 'tools/build-installer-actions.ps1')
try {
    $first = Build-Package 'first'
    $second = Build-Package 'second'
    Run-Msi @('/i', "`"$first`"") 'default.log'
    Assert-Location $defaultDirectory
    New-Item -ItemType Directory -Force -Path "$dataDirectory/Assets/themes/custom" | Out-Null
    $settings = Join-Path $dataDirectory 'Assets/settings.json'
    $customTheme = Join-Path $dataDirectory 'Assets/themes/custom/reader.json'
    Set-Content -LiteralPath $settings -Value '{"test":true}'
    Set-Content -LiteralPath $customTheme -Value '{"theme":"test"}'
    [FoliaInstallerCredentialTest]::Seed($credentialTarget)
    Run-Msi @('/x', "{$($products[0])}") 'uninstall-default.log'
    Assert (-not (Test-Path -LiteralPath "$defaultDirectory/Folia.exe")) 'Default uninstall left the program'
    Assert ((Test-Path -LiteralPath $settings) -and (Test-Path -LiteralPath $customTheme)) 'Normal uninstall removed user data'
    Assert ([FoliaInstallerCredentialTest]::Exists($credentialTarget)) 'Normal uninstall removed the credential'

    Run-Msi @('/i', "`"$first`"", "INSTALLFOLDER=`"$customDirectory`"") 'custom.log'
    Assert-Location $customDirectory
    Assert (-not (Test-Path -LiteralPath "$defaultDirectory/Folia.exe")) 'Custom installation also used the default directory'
    Run-Msi @('/i', "`"$second`"", 'FOLIAREMOVEUSERDATA=1') 'upgrade.log'
    Assert-Location $customDirectory
    Assert ((Test-Path -LiteralPath $settings) -and [FoliaInstallerCredentialTest]::Exists($credentialTarget)) 'Upgrade removed user data'
    Run-Msi @('/i', "`"$second`"", 'REINSTALL=ALL', 'REINSTALLMODE=omus', 'FOLIAREMOVEUSERDATA=1') 'repair.log'
    Assert-Location $customDirectory
    Assert ((Test-Path -LiteralPath $customTheme) -and [FoliaInstallerCredentialTest]::Exists($credentialTarget)) 'Repair removed user data'

    Run-Msi @('/i', "`"$first`"", "INSTALLFOLDER=`"$movedDirectory`"") 'relocate.log'
    Assert-Location $movedDirectory
    Assert (-not (Test-Path -LiteralPath "$customDirectory/Folia.exe")) 'Upgrade left the program in the previous directory'
    Run-Msi @('/x', "{$($products[0])}", 'FOLIAREMOVEUSERDATA=1') 'uninstall-custom.log'
    Assert (-not (Test-Path -LiteralPath "$movedDirectory/Folia.exe")) 'Custom uninstall left the program'
    Assert (-not (Test-Path -LiteralPath $dataDirectory)) 'Full uninstall left settings, resources or custom themes'
    Assert (-not [FoliaInstallerCredentialTest]::Exists($credentialTarget)) 'Full uninstall left the saved credential'
    Write-Host 'PASS: directory selection, custom paths, upgrade/repair/relocation, normal data retention and full data/credential removal.'
} finally {
    foreach ($product in $products) {
        $process = Start-Process msiexec.exe -ArgumentList @('/x', "{$product}", '/qn', '/norestart', 'FOLIAREMOVEUSERDATA=1') -WindowStyle Hidden -PassThru
        if (-not $process.WaitForExit(60000)) { throw "Test product cleanup timed out: $product" }
        $process.Refresh()
        Assert ($process.ExitCode -in @(0, 1605, 3010)) "Test product cleanup failed: $product"
    }
    if (Test-Path -LiteralPath "HKCU:\$registryKey") { Remove-Item -LiteralPath "HKCU:\$registryKey" -Recurse -Force }
    [FoliaInstallerCredentialTest]::Clear($credentialTarget)
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($installer)
}
