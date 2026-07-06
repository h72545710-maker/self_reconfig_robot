# ROS2 bridge

This folder contains an optional ROS2 package for the control prototype.

## Build

```bash
cd ros2
colcon build
source install/setup.bash
```

## Run

Start the control simulator first from the project root:

```bash
./build/robot_sim --ticks 60 --sleep-ms 200 --state-file build/control_state.json
```

Then publish the state file as ROS2 topics:

```bash
cd ros2
source install/setup.bash
ros2 launch self_reconfig_control control_bridge.launch.py state_file:=../build/control_state.json
```

Inspect the topics:

```bash
ros2 topic echo /self_reconfig/control_state
ros2 topic echo /self_reconfig/mission
ros2 topic echo /self_reconfig/modules
ros2 topic echo /self_reconfig/path
ros2 topic echo /self_reconfig/metrics
ros2 topic echo /self_reconfig/events
```

Inside the Docker environment, a quick check is available:

```bash
bash scripts/check_ros2_topics_in_docker.sh
```

## Interview note

The ROS2 bridge keeps the full JSON state on `/self_reconfig/control_state` and also splits key semantic fields into `/self_reconfig/mission`, `/self_reconfig/modules`, `/self_reconfig/path`, `/self_reconfig/metrics`, and `/self_reconfig/events`. This keeps the bridge easy to debug while making the ROS2 graph closer to a real robot software stack. A production version can replace these `std_msgs/String` slices with typed messages.
