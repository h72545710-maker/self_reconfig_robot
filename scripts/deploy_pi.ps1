param(
    [string]$Target = "yu@192.168.1.119",
    [string]$RemoteDir = "~/self_reconfig_robot",
    [switch]$Run,
    [switch]$RunUdpDemo,
    [switch]$RunOfflineDemo,
    [switch]$RunAckDemo,
    [switch]$RunTaskDemo
)

$ErrorActionPreference = "Stop"

Write-Host "Target: $Target"
Write-Host "Remote directory: $RemoteDir"

ssh $Target "mkdir -p $RemoteDir/include/robot $RemoteDir/src $RemoteDir/docs $RemoteDir/scripts"

scp CMakeLists.txt README.md $Target`:$RemoteDir/
scp include/robot/*.h $Target`:$RemoteDir/include/robot/
scp src/*.cpp $Target`:$RemoteDir/src/
scp docs/* $Target`:$RemoteDir/docs/
scp scripts/*.sh $Target`:$RemoteDir/scripts/

ssh $Target "cd $RemoteDir && chmod +x scripts/*.sh && bash scripts/build_native.sh"

if ($Run) {
    ssh $Target "cd $RemoteDir && ./build/robot_sim"
}

if ($RunUdpDemo) {
    ssh $Target "cd $RemoteDir && bash scripts/run_udp_demo.sh"
}

if ($RunOfflineDemo) {
    ssh $Target "cd $RemoteDir && bash scripts/run_offline_demo.sh"
}

if ($RunAckDemo) {
    ssh $Target "cd $RemoteDir && bash scripts/run_ack_demo.sh"
}

if ($RunTaskDemo) {
    ssh $Target "cd $RemoteDir && bash scripts/run_task_demo.sh"
}
