param([string]$BuildDirectory = (Join-Path $PSScriptRoot 'build/settings'),
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Release',
    [string]$OutputPath = (Join-Path $PSScriptRoot 'build/DiaTweaks-Windows-x64.zip'))
$ErrorActionPreference = 'Stop'
$stage = Join-Path $PSScriptRoot ('build/package-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage -Force > $null
foreach ($file in @('DiaTweaks.exe', 'DiaTweaksTap.dll', 'diatweaks-probe.exe')) {
    $source = Join-Path (Join-Path $BuildDirectory $Configuration) $file
    if (!(Test-Path -LiteralPath $source)) { throw "Build output missing: $file" }
    Copy-Item -LiteralPath $source -Destination (Join-Path $stage $file)
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'TESTER-README.txt') -Destination (Join-Path $stage 'README.txt')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'settings/assets/LICENSE.fluent-icons') -Destination (Join-Path $stage 'LICENSE.fluent-icons')
$revision = git -C $PSScriptRoot rev-parse HEAD
if ($LASTEXITCODE) { throw 'Could not identify the source commit.' }
$changes = git -C $PSScriptRoot status --porcelain --untracked-files=normal
if ($LASTEXITCODE) { throw 'Could not read source status.' }
$sourceStatus = if ($changes) { 'Local changes included' } else { 'Clean checkout' }
Set-Content -LiteralPath (Join-Path $stage 'BUILD.txt') -Value "Base commit: $revision`nSource status: $sourceStatus`nArchitecture: x64`nConfiguration: $Configuration" -Encoding utf8
$hashes = Get-ChildItem -LiteralPath $stage -File | Sort-Object Name | ForEach-Object { (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant() + '  ' + $_.Name }
Set-Content -LiteralPath (Join-Path $stage 'SHA256SUMS.txt') -Value $hashes -Encoding ascii
$smoke = Start-Process -FilePath (Join-Path $stage 'DiaTweaks.exe') -ArgumentList '--self-test' -WindowStyle Hidden -Wait -PassThru
if ($smoke.ExitCode) { throw "Packaged launcher check failed: $($smoke.ExitCode)" }
New-Item -ItemType Directory -Path (Split-Path $OutputPath) -Force > $null
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $OutputPath -Force
Write-Host "Tester ZIP: $OutputPath"
Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256
