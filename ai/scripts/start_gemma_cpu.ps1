# Run Gemma 4 E2B on the CPU with llama.cpp. Leave this window open during the workshop.
# Localhost only: the gateway talks to it; students never connect to it directly.
#   powershell -ExecutionPolicy Bypass -File ai\scripts\start_gemma_cpu.ps1
param(
    [string]$Dir = $(if ($env:AIOT_MODELS) { $env:AIOT_MODELS } else { Join-Path $env:USERPROFILE ".cache\aiot-workshop" }),
    [int]$Port = 8090  # 8080 is often used by the NI/LabVIEW web server.
)
$server = Join-Path $Dir "llama.cpp\llama-server.exe"
$model = Join-Path $Dir "gemma-4-E2B_q4_0-it.gguf"
if (-not (Test-Path $server) -or -not (Test-Path $model)) {
    Write-Error "Run ai\scripts\setup_gemma_cpu.ps1 first (looked in $Dir)"
    exit 1
}
# --no-mmproj: text only (no image encoder). --reasoning off: answer directly, no thinking tokens.
# -np 1: one request at a time is fastest on CPU; the gateway queues and caches answers.
& $server -m $model --host 127.0.0.1 --port $Port -c 4096 -np 1 --no-mmproj --reasoning off
