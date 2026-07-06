#!/usr/bin/env bash
set -euo pipefail

ROS_DISTRO_NAME="${ROS_DISTRO_NAME:-jazzy}"
ROS_PACKAGE="${ROS_PACKAGE:-ros-base}"
ROS_EXTRA_PACKAGES="${ROS_EXTRA_PACKAGES:-ros-${ROS_DISTRO_NAME}-demo-nodes-cpp ros-${ROS_DISTRO_NAME}-demo-nodes-py}"

if [[ ! -f /etc/os-release ]]; then
    echo "Cannot detect Linux distribution: /etc/os-release is missing." >&2
    exit 1
fi

. /etc/os-release

if [[ "${ID:-}" != "ubuntu" ]]; then
    echo "This installer is for Ubuntu 24.04. Detected: ${PRETTY_NAME:-unknown}." >&2
    echo "For Raspberry Pi OS, prefer Ubuntu 24.04 arm64 or use a ROS Docker image." >&2
    exit 1
fi

if [[ "${VERSION_ID:-}" != "24.04" ]]; then
    echo "ROS 2 ${ROS_DISTRO_NAME} binary packages are expected on Ubuntu 24.04." >&2
    echo "Detected: ${PRETTY_NAME:-unknown}. Please use Ubuntu 24.04 for this script." >&2
    exit 1
fi

echo "Installing ROS 2 ${ROS_DISTRO_NAME} ${ROS_PACKAGE} on ${PRETTY_NAME} ($(uname -m))"

sudo apt update
sudo apt install -y software-properties-common curl gnupg lsb-release locales

sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
export LANG=en_US.UTF-8

sudo add-apt-repository -y universe
sudo apt update

sudo install -m 0755 -d /etc/apt/keyrings
curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key |
    sudo tee /etc/apt/keyrings/ros-archive-keyring.gpg >/dev/null
sudo chmod 0644 /etc/apt/keyrings/ros-archive-keyring.gpg

echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu ${UBUNTU_CODENAME} main" |
    sudo tee /etc/apt/sources.list.d/ros2.list >/dev/null

sudo apt update
sudo apt install -y "ros-${ROS_DISTRO_NAME}-${ROS_PACKAGE}" ${ROS_EXTRA_PACKAGES} ros-dev-tools python3-argcomplete

if ! grep -q "/opt/ros/${ROS_DISTRO_NAME}/setup.bash" "$HOME/.bashrc"; then
    echo "source /opt/ros/${ROS_DISTRO_NAME}/setup.bash" >> "$HOME/.bashrc"
fi

set +u
source "/opt/ros/${ROS_DISTRO_NAME}/setup.bash"
set -u

echo
echo "ROS 2 installed:"
ros2 --version || true
echo
echo "Try:"
echo "  source /opt/ros/${ROS_DISTRO_NAME}/setup.bash"
echo "  ros2 topic list"
