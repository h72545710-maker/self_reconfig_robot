param(
    [int]$Ticks = 60,
    [int]$SleepMs = 260,
    [int]$Port = 8081,
    [int]$HoldSeconds = 120
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildDir = Join-Path $Root "build"
$LogDir = Join-Path $Root "run_logs"
$StateFile = Join-Path $BuildDir "control_state.json"

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
Remove-Item -Force -ErrorAction SilentlyContinue "$LogDir\control_*.log", "$LogDir\control_*.err", $StateFile

cmake -S $Root -B $BuildDir | Out-Host
cmake --build $BuildDir --config Release | Out-Host

$BinDir = Join-Path $BuildDir "Release"

$server = Start-Process -FilePath "python" `
    -ArgumentList "-m", "http.server", "$Port", "--bind", "127.0.0.1", "--directory", "$Root" `
    -RedirectStandardOutput "$LogDir\control_http.log" `
    -RedirectStandardError "$LogDir\control_http.err" `
    -PassThru -WindowStyle Hidden

Start-Sleep -Milliseconds 300

$url = "http://127.0.0.1:$Port/web/index.html?source=control_state.json"
Write-Host "Control visual dashboard: $url"
Start-Process $url

$sim = Start-Process -FilePath (Join-Path $BinDir "robot_sim.exe") `
    -ArgumentList "--ticks", "$Ticks", "--sleep-ms", "$SleepMs", "--state-file", "$StateFile" `
    -RedirectStandardOutput "$LogDir\control_sim.log" `
    -RedirectStandardError "$LogDir\control_sim.err" `
    -PassThru -WindowStyle Hidden

try {
    Wait-Process -Id $sim.Id
    Write-Host "`n===== control sim output ====="
    Get-Content "$LogDir\control_sim.log"
    Write-Host "`nDashboard will stay available for $HoldSeconds seconds."
    Start-Sleep -Seconds $HoldSeconds
}
finally {
    if (!$server.HasExited) {
        Stop-Process -Id $server.Id -Force
    }
}
