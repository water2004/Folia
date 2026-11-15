[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$ProgramRoot,

    [Parameter(Mandatory = $true)]
    [string]$AssetsRoot,

    [Parameter(Mandatory = $true)]
    [string]$OutputPath,

    [string[]]$ProgramLanguages = @('en-US', 'zh-CN')
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Get-DeploymentFiles.ps1')

function Get-StableId {
    param(
        [Parameter(Mandatory = $true)][string]$Prefix,
        [Parameter(Mandatory = $true)][string]$Value
    )

    $bytes = [System.Text.Encoding]::UTF8.GetBytes($Value.ToLowerInvariant())
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hash = $algorithm.ComputeHash($bytes)
    }
    finally {
        $algorithm.Dispose()
    }
    $hex = ([System.BitConverter]::ToString($hash)).Replace('-', '').Substring(0, 24)
    return "$Prefix$hex"
}

function Get-StableGuid {
    param(
        [Parameter(Mandatory = $true)][string]$Value
    )

    $bytes = [System.Text.Encoding]::UTF8.GetBytes("FoliaInstaller/$($Value.ToLowerInvariant())")
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hex = ([System.BitConverter]::ToString($algorithm.ComputeHash($bytes))).Replace('-', '')
    }
    finally {
        $algorithm.Dispose()
    }

    # Use a deterministic, UUID-shaped value. The fixed version/variant nibbles
    # make the generated component identity stable across machines and builds.
    return '{0}-{1}-5{2}-A{3}-{4}' -f `
        $hex.Substring(0, 8), `
        $hex.Substring(8, 4), `
        $hex.Substring(13, 3), `
        $hex.Substring(17, 3), `
        $hex.Substring(20, 12)
}

function Get-PayloadRecords {
    param([string]$Root, [string]$Kind)

    Get-DeploymentFiles -Root $Root -Kind $Kind -Languages $ProgramLanguages | ForEach-Object {
        $identity = "$Kind/$($_.Relative)"
        $_ | Add-Member -NotePropertyMembers @{
            ComponentId = Get-StableId -Prefix "Cmp$Kind" -Value $identity
            ComponentGuid = Get-StableGuid -Value $identity
            FileId = Get-StableId -Prefix "File$Kind" -Value $identity
        } -PassThru
    }
}

function Write-Component {
    param(
        [Parameter(Mandatory = $true)][System.Xml.XmlWriter]$Writer,
        [Parameter(Mandatory = $true)]$Record
    )

    $Writer.WriteStartElement('Component')
    $Writer.WriteAttributeString('Id', $Record.ComponentId)
    $Writer.WriteAttributeString('Guid', $Record.ComponentGuid)
    $Writer.WriteAttributeString('Bitness', 'always64')

    if ($Record.Kind -eq 'Asset' -and (
        ($Record.Directory -ieq 'themes' -and [System.IO.Path]::GetExtension($Record.Name) -ieq '.json') -or
        $Record.Relative -ieq 'latex/commands.json' -or $Record.Relative -ieq 'completion.json')) {
        # Built-in themes, command catalogs and prompt defaults are application resources.
        # Remove exactly this packaged file before InstallFiles, including on
        # repair, so MSI's unversioned-file preservation cannot retain schema 1.
        # Imported themes and settings are excluded from the payload entirely.
        $Writer.WriteStartElement('RemoveFile')
        $Writer.WriteAttributeString('Id', (Get-StableId -Prefix 'ReplaceBuiltIn' -Value $Record.Relative))
        $Writer.WriteAttributeString('Name', $Record.Name)
        $Writer.WriteAttributeString('On', 'install')
        $Writer.WriteEndElement()
    }

    $Writer.WriteStartElement('File')
    $Writer.WriteAttributeString('Id', $Record.FileId)
    $Writer.WriteAttributeString('Name', $Record.Name)
    $Writer.WriteAttributeString('Source', $Record.Source)
    $Writer.WriteEndElement()

    $Writer.WriteStartElement('RegistryValue')
    $Writer.WriteAttributeString('Root', 'HKCU')
    $Writer.WriteAttributeString('Key', 'Software\water2004\Folia\Installer\Components')
    $Writer.WriteAttributeString('Name', $Record.ComponentId)
    $Writer.WriteAttributeString('Type', 'integer')
    $Writer.WriteAttributeString('Value', '1')
    $Writer.WriteAttributeString('KeyPath', 'yes')
    $Writer.WriteEndElement()

    $Writer.WriteEndElement()
}

function Get-PayloadDirectories {
    param(
        [Parameter(Mandatory = $true)][object[]]$Records
    )

    $directories = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::Ordinal)
    foreach ($record in $Records) {
        if (-not $record.Directory) { continue }
        $parts = $record.Directory.Split('/')
        $current = ''
        foreach ($part in $parts) {
            if ($current) { $current = "$current/$part" }
            else { $current = $part }
            [void]$directories.Add($current)
        }
    }
    return @($directories) | Sort-Object
}

function Write-DirectoryCleanupComponent {
    param(
        [Parameter(Mandatory = $true)][System.Xml.XmlWriter]$Writer,
        [Parameter(Mandatory = $true)][string]$Kind,
        [Parameter(Mandatory = $true)][string]$RelativeDirectory
    )

    $componentId = Get-StableId -Prefix "Cmp${Kind}Dir" -Value "$Kind/directory/$RelativeDirectory"
    $Writer.WriteStartElement('Component')
    $Writer.WriteAttributeString('Id', $componentId)
    $Writer.WriteAttributeString('Guid', (Get-StableGuid -Value "$Kind/directory/$RelativeDirectory"))
    $Writer.WriteAttributeString('Bitness', 'always64')

    $Writer.WriteStartElement('RemoveFolder')
    $Writer.WriteAttributeString('Id', (Get-StableId -Prefix "Remove${Kind}Dir" -Value "$Kind/directory/$RelativeDirectory"))
    $Writer.WriteAttributeString('On', 'uninstall')
    $Writer.WriteEndElement()

    $Writer.WriteStartElement('RegistryValue')
    $Writer.WriteAttributeString('Root', 'HKCU')
    $Writer.WriteAttributeString('Key', 'Software\water2004\Folia\Installer\Components')
    $Writer.WriteAttributeString('Name', $componentId)
    $Writer.WriteAttributeString('Type', 'integer')
    $Writer.WriteAttributeString('Value', '1')
    $Writer.WriteAttributeString('KeyPath', 'yes')
    $Writer.WriteEndElement()
    $Writer.WriteEndElement()
}

function Write-DirectoryContents {
    param(
        [Parameter(Mandatory = $true)][System.Xml.XmlWriter]$Writer,
        [Parameter(Mandatory = $true)][object[]]$Records,
        [Parameter(Mandatory = $true)][string]$Kind,
        [string]$RelativeDirectory = ''
    )

    foreach ($record in @($Records | Where-Object Directory -CEQ $RelativeDirectory)) {
        Write-Component -Writer $Writer -Record $record
    }

    $childDirectories = @(
        $Records.Directory | ForEach-Object {
            if ($RelativeDirectory) {
                $prefix = "$RelativeDirectory/"
                if (-not $_.StartsWith($prefix, [System.StringComparison]::Ordinal)) { return }
                $remainder = $_.Substring($prefix.Length)
            }
            else {
                $remainder = $_
            }

            if (-not $remainder) { return }
            $childName = $remainder.Split('/')[0]
            if ($RelativeDirectory) { return "$RelativeDirectory/$childName" }
            return $childName
        } | Sort-Object -Unique
    )

    foreach ($child in $childDirectories) {
        $name = [System.IO.Path]::GetFileName($child)
        $Writer.WriteStartElement('Directory')
        $Writer.WriteAttributeString('Id', (Get-StableId -Prefix "Dir$Kind" -Value "$Kind/$child"))
        $Writer.WriteAttributeString('Name', $name)
        Write-DirectoryCleanupComponent -Writer $Writer -Kind $Kind -RelativeDirectory $child
        Write-DirectoryContents -Writer $Writer -Records $Records -Kind $Kind -RelativeDirectory $child
        $Writer.WriteEndElement()
    }
}

function Write-PayloadFragment {
    param(
        [Parameter(Mandatory = $true)][System.Xml.XmlWriter]$Writer,
        [Parameter(Mandatory = $true)][object[]]$Records,
        [Parameter(Mandatory = $true)][string]$Kind,
        [Parameter(Mandatory = $true)][string]$RootDirectoryId,
        [Parameter(Mandatory = $true)][string]$ComponentGroupId
    )

    $Writer.WriteStartElement('Fragment')
    $Writer.WriteStartElement('DirectoryRef')
    $Writer.WriteAttributeString('Id', $RootDirectoryId)
    Write-DirectoryContents -Writer $Writer -Records $Records -Kind $Kind
    $Writer.WriteEndElement()
    $Writer.WriteEndElement()

    $Writer.WriteStartElement('Fragment')
    $Writer.WriteStartElement('ComponentGroup')
    $Writer.WriteAttributeString('Id', $ComponentGroupId)
    foreach ($record in $Records) {
        $Writer.WriteStartElement('ComponentRef')
        $Writer.WriteAttributeString('Id', $record.ComponentId)
        $Writer.WriteEndElement()
    }
    foreach ($directory in (Get-PayloadDirectories -Records $Records)) {
        $Writer.WriteStartElement('ComponentRef')
        $Writer.WriteAttributeString('Id', (Get-StableId -Prefix "Cmp${Kind}Dir" -Value "$Kind/directory/$directory"))
        $Writer.WriteEndElement()
    }
    $Writer.WriteEndElement()
    $Writer.WriteEndElement()
}

$programRecords = Get-PayloadRecords -Root $ProgramRoot -Kind 'Program'
$assetRecords = Get-PayloadRecords -Root $AssetsRoot -Kind 'Asset'
if ($programRecords.Count -eq 0) { throw "No program files found under $ProgramRoot" }
if ($assetRecords.Count -eq 0) { throw "No asset files found under $AssetsRoot" }

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputPath)
$outputDirectory = [System.IO.Path]::GetDirectoryName($resolvedOutput)
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

$settings = [System.Xml.XmlWriterSettings]::new()
$settings.Indent = $true
$settings.Encoding = [System.Text.UTF8Encoding]::new($false)
$writer = [System.Xml.XmlWriter]::Create($resolvedOutput, $settings)
try {
    $writer.WriteStartDocument()
    $writer.WriteStartElement('Wix', 'http://wixtoolset.org/schemas/v4/wxs')
    Write-PayloadFragment -Writer $writer -Records $programRecords -Kind 'Program' -RootDirectoryId 'INSTALLFOLDER' -ComponentGroupId 'ProgramPayload'
    Write-PayloadFragment -Writer $writer -Records $assetRecords -Kind 'Asset' -RootDirectoryId 'ASSETFOLDER' -ComponentGroupId 'AssetPayload'
    $writer.WriteEndElement()
    $writer.WriteEndDocument()
}
finally {
    $writer.Dispose()
}

Write-Host "Generated WiX payload: $($programRecords.Count) program files, $($assetRecords.Count) asset files."
