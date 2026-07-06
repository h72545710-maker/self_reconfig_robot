#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
ROS_WS="${PROJECT_ROOT}/ros2"
ROS_DISTRO_NAME="${ROS_DISTRO_NAME:-jazzy}"

if [[ ! -f "/opt/ros/${ROS_DISTRO_NAME}/setup.bash" ]]; then
    echo "ROS 2 ${ROS_DISTRO_NAME} is not found under /opt/ros/${ROS_DISTRO_NAME}." >&2
    echo "Run scripts/install_ros2_jazzy_ubuntu.sh first." >&2
    exit 1
fi

set +u
source "/opt/ros/${ROS_DISTRO_NAME}/setup.bash"
set -u

cd "${ROS_WS}"
colcon build --symlink-install

set +u
source "${ROS_WS}/install/setup.bash"
set -u

echo
echo "ROS 2 bridge built."
echo "Run:"
echo "  source /opt/ros/${ROS_DISTRO_NAME}/setup.bash"
echo "  source ${ROS_WS}/install/setup.bash"
echo "  ros2 launch self_reconfig_control control_bridge.launch.py state_file:=${PROJECT_ROOT}/build/control_state.json"
