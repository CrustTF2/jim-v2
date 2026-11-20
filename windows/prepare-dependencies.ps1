$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$dependencyDir = Join-Path $root 'deps'
$downloadDir = Join-Path $root '.dependency-downloads'
New-Item -ItemType Directory -Force -Path $dependencyDir, $downloadDir | Out-Null
$dependencies = Get-Content -Raw (Join-Path $root 'dependency-sources.json') | ConvertFrom-Json
foreach ($dependency in $dependencies) {
    $destination = Join-Path $dependencyDir $dependency.name
    if (Test-Path -LiteralPath $destination) {
        Write-Host "Using existing $($dependency.name)"
        continue
    }
    $archive = Join-Path $downloadDir "$($dependency.name).tar.gz"
    Write-Host "Downloading $($dependency.name) $($dependency.tag)"
    Invoke-WebRequest -Uri $dependency.url -OutFile $archive
    $actual = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash.ToLowerInvariant()
    if ($actual -ne $dependency.sha256) { throw "Hash mismatch for $($dependency.name)" }
    New-Item -ItemType Directory -Path $destination | Out-Null
    & tar -xzf $archive -C $destination --strip-components=1
    if ($LASTEXITCODE -ne 0) { throw "Extraction failed for $($dependency.name)" }
}
