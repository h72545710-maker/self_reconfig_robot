param(
    [string]$Target = "yu@192.168.1.119",
    [string]$RemoteDir = "~/self_reconfig_robot"
)

$ErrorActionPreference = "Stop"

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$Scripts = @(
    "scripts/install_ros2_jazzy_ubuntu.sh",
    "scripts/install_ros2_jazzy_docker_ubuntu.sh",
    "scripts/run_ros2_jazzy_docker.sh",
    "scripts/build_ros2_bridge.sh",
    "scripts/build_ros2_bridge_in_docker.sh",
    "scripts/check_ros2_env.sh",
    "scripts/check_ros2_topics_in_docker.sh",
    "ros2/README.md",
    "ros2/self_reconfig_control/package.xml",
    "ros2/self_reconfig_control/CMakeLists.txt",
    "ros2/self_reconfig_control/src/control_state_publisher.cpp",
    "ros2/self_reconfig_control/launch/control_bridge.launch.py"
)

Write-Host "Target: $Target"
Write-Host "Remote directory: $RemoteDir"

ssh $Target "mkdir -p $RemoteDir/scripts $RemoteDir/ros2/self_reconfig_control/src $RemoteDir/ros2/self_reconfig_control/launch"

foreach ($Item in $Scripts) {
    $LocalPath = Join-Path $ProjectRoot $Item
    $RemotePath = "$RemoteDir/$($Item -replace '\\','/')"
    scp $LocalPath "${Target}:$RemotePath"
}

ssh $Target "chmod +x $RemoteDir/scripts/*.sh"

Write-Host ""
Write-Host "Deployed ROS2 scripts. On the Pi, run:"
Write-Host "  cd $RemoteDir"
Write-Host "  bash scripts/install_ros2_jazzy_ubuntu.sh"
Write-Host "  bash scripts/build_ros2_bridge.sh"
