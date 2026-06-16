param(
    [string]$Config = "Debug",
    [string]$BuildDir = "build-msvc",
    [string]$QtCMakeDir = "F:\Qt\6.6.3\6.6.3\msvc2019_64\lib\cmake",
    [string]$QtBinDir = "F:\Qt\6.6.3\6.6.3\msvc2019_64\bin",
    [switch]$SkipBuild,
    [switch]$BuildOnly,
    [switch]$RunTests,
    [switch]$StartBackend,
    [switch]$RestartBackend,
    [switch]$BackendOnly
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildPath = Join-Path $repoRoot $BuildDir
$appPath = Join-Path $buildPath "$Config\camclaw_qt_app.exe"

Set-Location $repoRoot

if (-not (Test-Path -LiteralPath $QtCMakeDir)) {
    throw "Qt CMake directory not found: $QtCMakeDir"
}

if (-not (Test-Path -LiteralPath $QtBinDir)) {
    throw "Qt bin directory not found: $QtBinDir"
}

if (-not $SkipBuild) {
    Get-Process camclaw_qt_app -ErrorAction SilentlyContinue | Stop-Process -Force

    Write-Host "Configuring CAMClaw Qt build..."
    cmake `
        -S $repoRoot `
        -B $buildPath `
        -G "Visual Studio 17 2022" `
        -A x64 `
        "-DCMAKE_PREFIX_PATH=$QtCMakeDir"

    Write-Host "Building camclaw_qt_app ($Config)..."
    cmake --build $buildPath --config $Config --target camclaw_qt_app

    if ($RunTests) {
        Write-Host "Building and running CAMClaw tests..."
        cmake --build $buildPath --config $Config
        $env:PATH = "$QtBinDir;$env:PATH"
        ctest --test-dir $buildPath -C $Config --output-on-failure
    }
}

if (-not (Test-Path -LiteralPath $appPath)) {
    throw "Qt app executable not found: $appPath"
}

if ($BuildOnly) {
    Write-Host "Build completed:"
    Write-Host $appPath
    exit 0
}

$env:PATH = "$QtBinDir;$env:PATH"

if ($StartBackend) {
    if ($RestartBackend) {
        Get-CimInstance Win32_Process |
            Where-Object { $_.Name -like 'python*' -and $_.CommandLine -like '*-m camclaw_agent_backend.server*' } |
            ForEach-Object {
                Write-Host "Stopping existing CAMClaw Agent backend process $($_.ProcessId)..."
                Stop-Process -Id $_.ProcessId -Force
            }
        Start-Sleep -Seconds 1
    }
    $backend = Get-NetTCPConnection -LocalAddress 127.0.0.1 -LocalPort 8765 -State Listen -ErrorAction SilentlyContinue
    if (-not $backend) {
        Write-Host "Starting CAMClaw Agent backend on http://127.0.0.1:8765 ..."
        Start-Process powershell `
            -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot 'run_agent_review_backend.ps1')) `
            -WorkingDirectory $repoRoot `
            -WindowStyle Hidden
        Start-Sleep -Seconds 2
    } else {
        Write-Host "CAMClaw Agent backend is already listening on 127.0.0.1:8765."
    }
}

if ($BackendOnly) {
    exit 0
}

Write-Host "Launching CAMClaw Qt CAM..."
Write-Host $appPath
& $appPath
