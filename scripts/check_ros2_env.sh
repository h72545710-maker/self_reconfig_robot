#!/usr/bin/env bash
set -euo pipefail

ROS_DISTRO_NAME="${ROS_DISTRO_NAME:-jazzy}"

echo "System:"
cat /etc/os-release | sed -n 's/^PRETTY_NAME=//p' | tr -d '"'
echo "Arch: $(uname -m)"
echo

if [[ -f "/opt/ros/${ROS_DISTRO_NAME}/setup.bash" ]]; then
    set +u
    source "/opt/ros/${ROS_DISTRO_NAME}/setup.bash"
    set -u
    echo "ROS_DISTRO=${ROS_DISTRO:-unknown}"
    echo "ros2: $(command -v ros2)"
    ros2 --version || true
    echo
    echo "ROS 2 environment looks available."
else
    echo "ROS 2 ${ROS_DISTRO_NAME} is not installed under /opt/ros/${ROS_DISTRO_NAME}."
    exit 1
fi
