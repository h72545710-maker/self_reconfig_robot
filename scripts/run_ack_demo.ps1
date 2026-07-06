param(
    [int]$Duration = 6
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildDir = Join-Path $Root "build"
$LogDir = Join-Path $Root "run_logs"

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
Remove-Item -Force -ErrorAction SilentlyContinue "$LogDir\*.log", "$LogDir\*.err"

cmake -S $Root -B $BuildDir | Out-Host
cmake --build $BuildDir --config Release | Out-Host

$BinDir = Join-Path $BuildDir "Release"

$master = Start-Process -FilePath (Join-Path $BinDir "master_node.exe") `
    -ArgumentList "--duration", "$Duration" `
    -RedirectStandardOutput "$LogDir\ack_master.log" `
    -RedirectStandardError "$LogDir\ack_master.err" `
    -PassThru -WindowStyle Hidden

Start-Sleep -Milliseconds 300

$module1 = Start-Process -FilePath (Join-Path $BinDir "module_node.exe") `
    -ArgumentList "--id", "1", "--battery", "95", "--x", "1", "--y", "1", "--duration", "$($Duration - 1)", "--drop-ack-once", "1" `
    -RedirectStandardOutput "$LogDir\ack_module1.log" `
    -RedirectStandardError "$LogDir\ack_module1.err" `
    -PassThru -WindowStyle Hidden

$module2 = Start-Process -FilePath (Join-Path $BinDir "module_node.exe") `
    -ArgumentList "--id", "2", "--battery", "88", "--x", "1", "--y", "2", "--duration", "$($Duration - 1)" `
    -RedirectStandardOutput "$LogDir\ack_module2.log" `
    -RedirectStandardError "$LogDir\ack_module2.err" `
    -PassThru -WindowStyle Hidden

Wait-Process -Id $master.Id, $module1.Id, $module2.Id

Write-Host "`n===== master output ====="
Get-Content "$LogDir\ack_master.log"

Write-Host "`n===== module output ====="
Get-Content "$LogDir\ack_module1.log"
Get-Content "$LogDir\ack_module2.log"
