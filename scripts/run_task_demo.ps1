param(
    [int]$Duration = 10
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
    -RedirectStandardOutput "$LogDir\task_master.log" `
    -RedirectStandardError "$LogDir\task_master.err" `
    -PassThru -WindowStyle Hidden

Start-Sleep -Milliseconds 300

$module1 = Start-Process -FilePath (Join-Path $BinDir "module_node.exe") `
    -ArgumentList "--id", "1", "--battery", "95", "--x", "1", "--y", "1", "--duration", "$($Duration - 2)", "--silent-after", "3" `
    -RedirectStandardOutput "$LogDir\task_module1.log" `
    -RedirectStandardError "$LogDir\task_module1.err" `
    -PassThru -WindowStyle Hidden

$module2 = Start-Process -FilePath (Join-Path $BinDir "module_node.exe") `
    -ArgumentList "--id", "2", "--battery", "88", "--x", "1", "--y", "2", "--duration", "$($Duration - 2)" `
    -RedirectStandardOutput "$LogDir\task_module2.log" `
    -RedirectStandardError "$LogDir\task_module2.err" `
    -PassThru -WindowStyle Hidden

$module3 = Start-Process -FilePath (Join-Path $BinDir "module_node.exe") `
    -ArgumentList "--id", "3", "--battery", "80", "--x", "1", "--y", "3", "--duration", "$($Duration - 2)" `
    -RedirectStandardOutput "$LogDir\task_module3.log" `
    -RedirectStandardError "$LogDir\task_module3.err" `
    -PassThru -WindowStyle Hidden

Wait-Process -Id $master.Id, $module1.Id, $module2.Id, $module3.Id

Write-Host "`n===== master output ====="
Get-Content "$LogDir\task_master.log"

Write-Host "`n===== module output ====="
Get-Content "$LogDir\task_module1.log"
Get-Content "$LogDir\task_module2.log"
Get-Content "$LogDir\task_module3.log"
