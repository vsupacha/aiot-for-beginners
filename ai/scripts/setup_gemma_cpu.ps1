# Download llama.cpp (Windows CPU build) and Gemma 4 E2B 4-bit (QAT) once, before workshop day.
# About 3.4 GB. Files go outside Dropbox/OneDrive so they are not synced.
#   powershell -ExecutionPolicy Bypass -File ai\scripts\setup_gemma_cpu.ps1
param(
    [string]$Dir = $(if ($env:AIOT_MODELS) { $env:AIOT_MODELS } else { Join-Path $env:USERPROFILE ".cache\aiot-workshop" }),
    [string]$LlamaBuild = "b11234"  # Build tested on 2026-09-28; use a newer tag if this one is removed.
)
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force $Dir | Out-Null

$server = Join-Path $Dir "llama.cpp\llama-server.exe"
if (-not (Test-Path $server)) {
    $zip = Join-Path $Dir "llama-cpu.zip"
    $url = "https://github.com/ggml-org/llama.cpp/releases/download/$LlamaBuild/llama-$LlamaBuild-bin-win-cpu-x64.zip"
    Write-Host "Downloading llama.cpp $LlamaBuild (~20 MB)"
    curl.exe -L --fail -o $zip $url
    if ($LASTEXITCODE -ne 0) { throw "llama.cpp download failed: $url" }
    Expand-Archive -Force $zip (Join-Path $Dir "llama.cpp")
}

$model = Join-Path $Dir "gemma-4-E2B_q4_0-it.gguf"
if (-not (Test-Path $model) -or (Get-Item $model).Length -lt 3000000000) {
    Write-Host "Downloading Gemma 4 E2B q4_0 (3.35 GB, Apache-2.0, no Hugging Face login needed)"
    curl.exe -L --fail -C - -o $model "https://huggingface.co/google/gemma-4-E2B-it-qat-q4_0-gguf/resolve/main/gemma-4-E2B_q4_0-it.gguf"
    if ($LASTEXITCODE -ne 0) { throw "model download failed; run again to resume" }
}
Write-Host "Ready: $Dir"
Write-Host "Next: powershell -ExecutionPolicy Bypass -File ai\scripts\start_gemma_cpu.ps1"
