[CmdletBinding()]
param([switch]$Update)
$ErrorActionPreference = 'Stop'
$toolDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = [IO.Path]::GetFullPath((Join-Path $toolDirectory '../..'))
$grammar = Join-Path $root 'src/math/editor/tree-sitter'
$candidate = Join-Path $root 'build/math-editor-grammar'
New-Item -ItemType Directory -Force -Path $candidate | Out-Null
& npm ci --prefix $toolDirectory --include=dev --no-audit --no-fund
if ($LASTEXITCODE -ne 0) { throw 'Math editing grammar tools restore failed.' }
foreach ($name in @('grammar.js', 'tree-sitter.json')) {
    Copy-Item -LiteralPath (Join-Path $grammar $name) -Destination (Join-Path $candidate $name) -Force
}
Push-Location $candidate
try {
    & (Join-Path $toolDirectory 'node_modules/.bin/tree-sitter.cmd') generate --abi 15
    if ($LASTEXITCODE -ne 0) { throw 'Math editing grammar generation failed.' }
} finally { Pop-Location }
foreach ($name in @('parser.c', 'node-types.json')) {
    $generated = Join-Path $candidate "src/$name"
    $destination = Join-Path $grammar "src/$name"
    if ($Update) { Copy-Item -LiteralPath $generated -Destination $destination -Force }
    elseif ([IO.File]::ReadAllText($generated).Replace("`r`n", "`n") -cne [IO.File]::ReadAllText($destination).Replace("`r`n", "`n")) {
        throw "Math editing grammar output is stale: $name. Run generate.ps1 -Update after reviewing grammar changes."
    }
}
Write-Output 'Math editing grammar outputs match tree-sitter-cli 0.25.10.'
