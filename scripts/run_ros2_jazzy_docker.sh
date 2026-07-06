#!/usr/bin/env bash
set -euo pipefail

ROS_IMAGE="${ROS_IMAGE:-ros:jazzy-ros-base}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

docker run --rm -it \
    --network host \
    --ipc host \
    -v "${PROJECT_ROOT}:/workspace/self_reconfig_robot" \
    -w /workspace/self_reconfig_robot \
    "${ROS_IMAGE}" \
    bash -lc 'source /opt/ros/jazzy/setup.bash &&
              apt update &&
              apt install -y python3-colcon-common-extensions build-essential cmake &&
              echo "ROS_DISTRO=$ROS_DISTRO" &&
              echo "Workspace mounted at /workspace/self_reconfig_robot" &&
              exec bash'
