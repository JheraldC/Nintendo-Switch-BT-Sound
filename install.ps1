param([switch]$CheckOnly)
$btOldErrorPreference = $ErrorActionPreference
$ErrorActionPreference = 'Stop'
$btCache = if ($env:BT_SOUND_CACHE) { [IO.Path]::GetFullPath($env:BT_SOUND_CACHE) } else { Join-Path ([Environment]::GetFolderPath('UserProfile')) '.bt-sound' }
$btTemp = $null
function Assert-BtChild([string]$Path) {
    $prefix = [IO.Path]::GetFullPath($btCache).TrimEnd('\') + '\'
    if (-not [IO.Path]::GetFullPath($Path).StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) { throw 'Path outside installer cache' }
}
function Get-BtVerified([string]$Url, [string]$Path, [string]$Sha) {
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $Path
    if ((Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Sha) { throw 'Download SHA-256 mismatch' }
}
try {
    if (-not [Environment]::Is64BitOperatingSystem) { throw 'Windows x64 is required' }
    New-Item -ItemType Directory -Path $btCache -Force | Out-Null
    $btTemp = Join-Path $btCache ('bootstrap-' + [Guid]::NewGuid().ToString('N'))
    Assert-BtChild $btTemp
    New-Item -ItemType Directory -Path $btTemp | Out-Null
    $btPythonRoot = Join-Path $btCache 'python-20261110-windows-x86_64'
    $btPython = Join-Path $btPythonRoot 'python/python.exe'
    if (-not (Test-Path -LiteralPath $btPython)) {
        if (Test-Path -LiteralPath $btPythonRoot) { throw 'Incomplete Python cache; review the cache directory' }
        Write-Host 'Preparing portable Python (first launch only)...'
        $btArchive = Join-Path $btTemp 'python.tar.gz'
        Get-BtVerified 'https://github.com/astral-sh/python-build-standalone/releases/download/20261110/cpython-3.12.16%2B20261110-x86_64-pc-windows-msvc-install_only_stripped.tar.gz' $btArchive '355ee61b8207b91574491b1c6dc9b0ebb4408fc56bfafe498f3ef439314b52dc'
        $btExtract = Join-Path $btTemp 'runtime'
        New-Item -ItemType Directory -Path $btExtract | Out-Null
        $btTar = Join-Path $env:SystemRoot 'System32/tar.exe'
        if (-not (Test-Path -LiteralPath $btTar)) { throw 'Windows tar.exe is required (Windows 10/11)' }
        & $btTar -xzf $btArchive -C $btExtract
        if ($LASTEXITCODE -ne 0) { throw 'Python extraction failed' }
        Assert-BtChild $btExtract
        Assert-BtChild $btPythonRoot
        Move-Item -LiteralPath $btExtract -Destination $btPythonRoot
    }
    $btBootstrap = Join-Path $btTemp 'bootstrap.py'
    Get-BtVerified 'https://github.com/JheraldC/Nintendo-Switch-BT-Sound/releases/download/v1.2.0/bootstrap-v1.2.0.py' $btBootstrap 'baa57ec2c9a9f282f1e05e8019aa8c5cf9441ade1bc1b173248e23d927912df5'
    $btLaunchArgs = @()
    if ($CheckOnly) { $btLaunchArgs += '--smoke-test' }
    & $btPython $btBootstrap @btLaunchArgs
    if ($LASTEXITCODE -ne 0) { throw 'Installer did not complete; see the preceding message' }
} finally {
    if ($btTemp -and (Test-Path -LiteralPath $btTemp)) {
        Assert-BtChild $btTemp
        Remove-Item -LiteralPath $btTemp -Recurse -Force
    }
    $ErrorActionPreference = $btOldErrorPreference
}
