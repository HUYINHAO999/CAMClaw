$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$env:PYTHONPATH = Join-Path $repoRoot "python_backend"
$env:PYTHONDONTWRITEBYTECODE = "1"

if (-not $env:CAMCLAW_LLM_BASE_URL) {
    $env:CAMCLAW_LLM_BASE_URL = "https://jspin.cn"
}

if (-not $env:CAMCLAW_LLM_MODEL) {
    $env:CAMCLAW_LLM_MODEL = "gpt-5.5"
}

Write-Host "Starting CAMClaw Agent backend at http://127.0.0.1:8765/agent-review"
Write-Host "LLM base URL: $env:CAMCLAW_LLM_BASE_URL"
Write-Host "LLM model: $env:CAMCLAW_LLM_MODEL"
Write-Host "Set CAMCLAW_LLM_API_KEY before running real planning requests."

python -m camclaw_agent_backend.server
