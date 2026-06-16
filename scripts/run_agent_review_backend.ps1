$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$localEnvPath = Join-Path $repoRoot ".env.local"
if (Test-Path -LiteralPath $localEnvPath) {
    Get-Content -LiteralPath $localEnvPath | ForEach-Object {
        $line = $_.Trim()
        if (-not $line -or $line.StartsWith("#")) {
            return
        }
        $separator = $line.IndexOf("=")
        if ($separator -lt 1) {
            return
        }
        $name = $line.Substring(0, $separator).Trim()
        $value = $line.Substring($separator + 1).Trim()
        if (($value.StartsWith('"') -and $value.EndsWith('"')) -or ($value.StartsWith("'") -and $value.EndsWith("'"))) {
            $value = $value.Substring(1, $value.Length - 2)
        }
        [Environment]::SetEnvironmentVariable($name, $value, "Process")
    }
}

$env:PYTHONPATH = Join-Path $repoRoot "python_backend"
$env:PYTHONDONTWRITEBYTECODE = "1"

if (-not $env:CAMCLAW_LLM_BASE_URL) {
    $env:CAMCLAW_LLM_BASE_URL = "https://jspin.cn"
}

if (-not $env:CAMCLAW_LLM_MODEL) {
    $env:CAMCLAW_LLM_MODEL = "gpt-5.5"
}

Write-Host "Starting CAMClaw Agent backend at http://127.0.0.1:8765/agent-review/"
Write-Host "LLM base URL: $env:CAMCLAW_LLM_BASE_URL"
Write-Host "LLM model: $env:CAMCLAW_LLM_MODEL"
if ($env:CAMCLAW_LLM_API_KEY) {
    Write-Host "LLM API key: configured"
} else {
    Write-Host "LLM API key: missing; set CAMCLAW_LLM_API_KEY in .env.local"
}

python -m camclaw_agent_backend.server
