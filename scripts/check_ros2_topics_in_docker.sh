#!/usr/bin/env bash
set -euo pipefail

if [[ ! -f /opt/ros/jazzy/setup.bash ]]; then
    echo "This script must run inside the ROS2 Jazzy container." >&2
    exit 1
fi

set +u
source /opt/ros/jazzy/setup.bash
if [[ -f /workspace/self_reconfig_robot/ros2/install/setup.bash ]]; then
    source /workspace/self_reconfig_robot/ros2/install/setup.bash
fi
set -u

echo "ROS2 topics:"
ros2 topic list -t | grep self_reconfig || true

echo
echo "Expected topics:"
echo "  /self_reconfig/control_state [std_msgs/msg/String]"
echo "  /self_reconfig/mission       [std_msgs/msg/String]"
echo "  /self_reconfig/modules       [std_msgs/msg/String]"
echo "  /self_reconfig/path          [std_msgs/msg/String]"
echo "  /self_reconfig/metrics       [std_msgs/msg/String]"
echo "  /self_reconfig/events        [std_msgs/msg/String]"

echo
echo "Mission sample:"
timeout 5 ros2 topic echo --once /self_reconfig/mission || true

echo
echo "Modules sample:"
timeout 5 ros2 topic echo --once /self_reconfig/modules || true

echo
echo "Metrics sample:"
timeout 5 ros2 topic echo --once /self_reconfig/metrics || true
