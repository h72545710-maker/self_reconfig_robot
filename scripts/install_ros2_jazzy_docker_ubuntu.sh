#!/usr/bin/env bash
set -euo pipefail

ROS_IMAGE="${ROS_IMAGE:-ros:jazzy-ros-base}"
ROS_IMAGE_MIRROR_PREFIX="${ROS_IMAGE_MIRROR_PREFIX:-docker.m.daocloud.io/library}"

if [[ ! -f /etc/os-release ]]; then
    echo "Cannot detect Linux distribution." >&2
    exit 1
fi

. /etc/os-release

echo "Host: ${PRETTY_NAME:-unknown} ($(uname -m))"
echo "ROS image: ${ROS_IMAGE}"

if ! command -v docker >/dev/null 2>&1; then
    echo "Installing Docker from Ubuntu apt repository..."
    sudo apt update
    sudo apt install -y docker.io
    sudo systemctl enable --now docker
fi

if ! groups "$USER" | grep -q '\bdocker\b'; then
    echo "Adding ${USER} to docker group..."
    sudo usermod -aG docker "$USER"
    echo
    echo "Please log out and log in again, then rerun this script:"
    echo "  bash scripts/install_ros2_jazzy_docker_ubuntu.sh"
    exit 0
fi

if ! docker pull "${ROS_IMAGE}"; then
    echo
    echo "Docker Hub pull failed. Trying mirror prefix: ${ROS_IMAGE_MIRROR_PREFIX}"
    MIRROR_IMAGE="${ROS_IMAGE_MIRROR_PREFIX}/${ROS_IMAGE}"
    docker pull "${MIRROR_IMAGE}"
    docker tag "${MIRROR_IMAGE}" "${ROS_IMAGE}"
fi

echo
echo "Docker ROS2 environment is ready."
echo "Run:"
echo "  bash scripts/run_ros2_jazzy_docker.sh"
