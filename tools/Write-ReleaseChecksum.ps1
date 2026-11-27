param([Parameter(Mandatory)][string]$Path)

$file = Get-Item -LiteralPath $Path
$hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$($file.FullName).sha256", "$hash  $($file.Name)`n", [Text.UTF8Encoding]::new($false))
