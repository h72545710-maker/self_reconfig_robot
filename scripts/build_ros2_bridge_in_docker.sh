#!/usr/bin/env bash
set -euo pipefail

if [[ ! -f /opt/ros/jazzy/setup.bash ]]; then
    echo "This script must run inside a ROS2 Jazzy container or environment." >&2
    exit 1
fi

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

set +u
source /opt/ros/jazzy/setup.bash
set -u

cd "${PROJECT_ROOT}/ros2"
colcon build --symlink-install

echo
echo "Bridge built. Try:"
echo "  source /opt/ros/jazzy/setup.bash"
echo "  source /workspace/self_reconfig_robot/ros2/install/setup.bash"
echo "  ros2 launch self_reconfig_control control_bridge.launch.py state_file:=/workspace/self_reconfig_robot/build/control_state.json"
