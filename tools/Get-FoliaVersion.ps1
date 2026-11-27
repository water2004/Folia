$manifest = [xml](Get-Content -LiteralPath (Join-Path $PSScriptRoot '../src/app-winui/Package.appxmanifest') -Raw)
([version]$manifest.Package.Identity.Version).ToString(3)
