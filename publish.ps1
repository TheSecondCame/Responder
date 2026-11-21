param(
    [string]$ExePath = (Join-Path $PSScriptRoot 'x64/Release/Server.exe'),
    [string]$OutputPath = (Join-Path $PSScriptRoot 'Responder-x64.zip')
)
$ErrorActionPreference = 'Stop'

function Assert-Asset([string]$Reference, [string]$Source, [string]$BaseDirectory) {
    $path = [Uri]::UnescapeDataString(($Reference -split '[?#]', 2)[0])
    if (!$path -or $path -match '^(?:[a-z][a-z0-9+.-]*:|//)') { return }
    $directory = $BaseDirectory
    if ($path.StartsWith('/')) {
        $directory = $PSScriptRoot
        $path = $path.TrimStart('/')
    }
    if (!(Test-Path -LiteralPath (Join-Path $directory $path) -PathType Leaf)) {
        throw "Missing asset '$Reference' in $Source"
    }
}

$assets = Join-Path $PSScriptRoot 'assets'
$sourceFiles = @(Get-ChildItem (Join-Path $PSScriptRoot 'Server') -Filter '*.cpp')
$sourceFiles += @(Get-ChildItem (Join-Path $assets 'js') -Filter '*.js')
foreach ($file in $sourceFiles) {
    $source = Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8
    foreach ($match in [regex]::Matches($source, '["''](/assets/[^"''?#\r\n]+)')) {
        Assert-Asset $match.Groups[1].Value $file.Name $PSScriptRoot
    }
}
foreach ($file in Get-ChildItem $assets -Recurse -Filter '*.css') {
    $source = Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8
    foreach ($match in [regex]::Matches($source, 'url\(\s*(?:"([^"]+)"|''([^'']+)''|([^)"'']+))\s*\)', 'IgnoreCase')) {
        $reference = ($match.Groups[1].Value + $match.Groups[2].Value + $match.Groups[3].Value).Trim()
        Assert-Asset $reference $file.Name $file.DirectoryName
    }
}

$exe = Get-Item -LiteralPath $ExePath
$target = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputPath)
if ([IO.Path]::GetExtension($target) -ne '.zip') { throw 'OutputPath must end with .zip' }
$stage = Join-Path ([IO.Path]::GetTempPath()) ([guid]::NewGuid().ToString('N'))
$tmpZip = Join-Path ([IO.Path]::GetDirectoryName($target)) (([guid]::NewGuid().ToString('N')) + '.zip')
try {
    New-Item -ItemType Directory -Path $stage | Out-Null
    Copy-Item -LiteralPath $exe.FullName -Destination (Join-Path $stage 'Server.exe')
    Copy-Item -LiteralPath $assets -Destination $stage -Recurse
    $bytes = (Get-ChildItem $stage -Recurse -File | Measure-Object -Property Length -Sum).Sum
    if ($bytes -gt 12000000) { throw "Unpacked deployment exceeds 12 MB: $bytes bytes" }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::CreateFromDirectory($stage, $tmpZip)
    $zipBytes = (Get-Item -LiteralPath $tmpZip).Length
    if ($zipBytes -gt 12000000) { throw "Deployment ZIP exceeds 12 MB: $zipBytes bytes" }
    Move-Item -LiteralPath $tmpZip -Destination $target -Force
    Write-Host "Published $target ($zipBytes bytes ZIP, $bytes bytes unpacked)"
} finally {
    Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $tmpZip -Force -ErrorAction SilentlyContinue
}
