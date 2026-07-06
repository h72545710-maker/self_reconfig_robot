param(
    [int]$Duration = 45,
    [int]$Port = 8080
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildDir = Join-Path $Root "build"
$LogDir = Join-Path $Root "run_logs"
$StateFile = Join-Path $BuildDir "state.json"

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
Remove-Item -Force -ErrorAction SilentlyContinue "$LogDir\visual_*.log", "$LogDir\visual_*.err", $StateFile

cmake -S $Root -B $BuildDir | Out-Host
cmake --build $BuildDir --config Release | Out-Host

$BinDir = Join-Path $BuildDir "Release"

$server = Start-Process -FilePath "python" `
    -ArgumentList "-m", "http.server", "$Port", "--bind", "127.0.0.1", "--directory", "$Root" `
    -RedirectStandardOutput "$LogDir\visual_http.log" `
    -RedirectStandardError "$LogDir\visual_http.err" `
    -PassThru -WindowStyle Hidden

$master = Start-Process -FilePath (Join-Path $BinDir "master_node.exe") `
    -ArgumentList "--duration", "$Duration", "--state-file", "$StateFile" `
    -RedirectStandardOutput "$LogDir\visual_master.log" `
    -RedirectStandardError "$LogDir\visual_master.err" `
    -PassThru -WindowStyle Hidden

Start-Sleep -Milliseconds 300

$module1 = Start-Process -FilePath (Join-Path $BinDir "module_node.exe") `
    -ArgumentList "--id", "1", "--battery", "95", "--x", "1", "--y", "1", "--duration", "$($Duration - 2)", "--silent-after", "3" `
    -RedirectStandardOutput "$LogDir\visual_module1.log" `
    -RedirectStandardError "$LogDir\visual_module1.err" `
    -PassThru -WindowStyle Hidden

$module2 = Start-Process -FilePath (Join-Path $BinDir "module_node.exe") `
    -ArgumentList "--id", "2", "--battery", "88", "--x", "1", "--y", "2", "--duration", "$($Duration - 2)" `
    -RedirectStandardOutput "$LogDir\visual_module2.log" `
    -RedirectStandardError "$LogDir\visual_module2.err" `
    -PassThru -WindowStyle Hidden

$module3 = Start-Process -FilePath (Join-Path $BinDir "module_node.exe") `
    -ArgumentList "--id", "3", "--battery", "80", "--x", "1", "--y", "3", "--duration", "$($Duration - 2)" `
    -RedirectStandardOutput "$LogDir\visual_module3.log" `
    -RedirectStandardError "$LogDir\visual_module3.err" `
    -PassThru -WindowStyle Hidden

$url = "http://127.0.0.1:$Port/web/index.html"
Write-Host "Visual dashboard: $url"
Start-Process $url

try {
    Wait-Process -Id $master.Id, $module1.Id, $module2.Id, $module3.Id
}
finally {
    if (!$server.HasExited) {
        Stop-Process -Id $server.Id -Force
    }
}

Write-Host "`n===== master output ====="
Get-Content "$LogDir\visual_master.log"
