function Test-CultureDirectory {
    param([Parameter(Mandatory = $true)][string]$Name)

    try {
        [void][System.Globalization.CultureInfo]::GetCultureInfo($Name)
        return $true
    }
    catch {
        return $false
    }
}

function Get-DeploymentFiles {
    param(
        [Parameter(Mandatory = $true)][string]$Root,
        [Parameter(Mandatory = $true)][ValidateSet('Program', 'Asset')]
        [string]$Kind,

        [string[]]$Languages = @('en-US', 'zh-CN')
    )

    $programLanguageLookup = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
    foreach ($language in $Languages) {
        [void]$programLanguageLookup.Add($language)
    }

    $resolvedRoot = [System.IO.Path]::GetFullPath($Root)
    $rootPrefix = $resolvedRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    $assetPrefix = [System.IO.Path]::Combine($resolvedRoot, 'Assets') + [System.IO.Path]::DirectorySeparatorChar
    $buildOnlyExtensions = @('.exp', '.ilk', '.iobj', '.ipdb', '.lib', '.pdb')

    return @(
        Get-ChildItem -LiteralPath $resolvedRoot -Recurse -File | ForEach-Object {
            $relative = $_.FullName.Substring($rootPrefix.Length).Replace('\', '/')

            if ($Kind -eq 'Program') {
                if ($_.FullName.StartsWith($assetPrefix, [System.StringComparison]::OrdinalIgnoreCase)) { return }
                if ($buildOnlyExtensions -contains $_.Extension.ToLowerInvariant()) { return }
                if ($relative -in @('FoliaMathWorkerProbe.exe', 'FoliaMathLayoutProbe.exe')) { return }
                $topDirectory = $relative.Split('/')[0]
                if ($relative.Contains('/') -and (Test-CultureDirectory -Name $topDirectory) -and
                    -not $programLanguageLookup.Contains($topDirectory)) { return }
            }
            else {
                if ($relative -ieq 'settings.json' -or $relative -ieq 'settings.json.tmp') { return }
                if ($relative -ieq 'latex/commands.user.json' -or $relative -ieq 'latex/commands.user.json.tmp') { return }
                if ($relative.StartsWith('themes/custom/', [System.StringComparison]::OrdinalIgnoreCase)) { return }
            }

            $directory = [System.IO.Path]::GetDirectoryName($relative)
            if ($null -eq $directory) { $directory = '' }
            $directory = $directory.Replace('\', '/')

            [pscustomobject]@{
                Kind = $Kind
                Source = $_.FullName
                Relative = $relative
                Directory = $directory
                Name = $_.Name
            }
        }
    ) | Sort-Object Relative
}
