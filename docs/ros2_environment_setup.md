# ROS2 环境安装说明

## 推荐路线

建议在树莓派或 WSL/Ubuntu 上安装 ROS2 Jazzy：

```text
Ubuntu 24.04 64-bit + ROS2 Jazzy + ros-base
```

原因：

- ROS2 Jazzy 是 LTS 版本，适合学习和长期项目维护。
- 官方二进制包面向 Ubuntu 24.04 Noble，安装成本最低。
- 树莓派上建议使用 64 位 Ubuntu，这样可以走官方二进制包路线。

## 不推荐的路线

- Windows 原生安装：可以装，但调试、依赖、串口、网络发现都更麻烦。
- Raspberry Pi OS 直接 apt 安装 ROS2：官方支持层级较低，容易遇到依赖问题。
- Ubuntu 22.04 装 Jazzy：不推荐。Ubuntu 22.04 更适合 ROS2 Humble。
- Ubuntu 25.10 直接 apt 装 Jazzy/Kilted：不推荐。当前项目学习阶段建议用 Docker 或重装 Ubuntu 24.04。

## 如果当前是 Ubuntu 25.10

你的树莓派如果显示：

```text
VERSION_ID="25.10"
VERSION_CODENAME=questing
```

推荐二选一：

1. 重装 Ubuntu 24.04 LTS arm64，然后安装 ROS2 Jazzy。这是最稳的长期路线。
2. 暂时不重装，用 Docker 跑 `ros:jazzy-ros-base`，适合先学 ROS2 topic/node/launch/colcon。

Docker 路线：

```bash
cd ~/self_reconfig_robot
bash scripts/install_ros2_jazzy_docker_ubuntu.sh
```

如果脚本提示需要重新登录，退出 SSH 后重新登录，再执行一次：

```bash
bash scripts/install_ros2_jazzy_docker_ubuntu.sh
```

进入 ROS2 Jazzy 容器：

```bash
bash scripts/run_ros2_jazzy_docker.sh
```

在容器里测试：

```bash
ros2 topic list
```

编译本项目 ROS2 bridge：

```bash
bash scripts/build_ros2_bridge_in_docker.sh
```

## 在树莓派/Ubuntu 上安装

进入项目目录：

```bash
cd ~/self_reconfig_robot
```

安装 ROS2 Jazzy：

```bash
bash scripts/install_ros2_jazzy_ubuntu.sh
```

检查环境：

```bash
bash scripts/check_ros2_env.sh
```

编译本项目的 ROS2 bridge：

```bash
bash scripts/build_ros2_bridge.sh
```

## 从 Windows 部署脚本到树莓派

在 Windows PowerShell 项目目录下运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\deploy_ros2_scripts_pi.ps1
```

部署后在树莓派上运行：

```bash
cd ~/self_reconfig_robot
bash scripts/install_ros2_jazzy_ubuntu.sh
bash scripts/build_ros2_bridge.sh
```

## 验证 ROS2

终端 1：

```bash
source /opt/ros/jazzy/setup.bash
ros2 run demo_nodes_cpp talker
```

终端 2：

```bash
source /opt/ros/jazzy/setup.bash
ros2 run demo_nodes_py listener
```

如果能看到 listener 收到 talker 消息，说明 ROS2 基础通信正常。

## 运行本项目 ROS2 bridge

先启动控制仿真生成状态文件：

```bash
./build/robot_sim --ticks 60 --sleep-ms 200 --state-file build/control_state.json
```

再启动 ROS2 发布节点：

```bash
source /opt/ros/jazzy/setup.bash
source ros2/install/setup.bash
ros2 launch self_reconfig_control control_bridge.launch.py state_file:=build/control_state.json
```

查看 topic：

```bash
ros2 topic echo /self_reconfig/control_state
```

## 面试表达

这一步可以这样讲：

> 项目主体控制逻辑保持 C++ 独立实现，同时新增 ROS2 适配包，把控制状态发布为 ROS2 topic。这样既能保留对底层通信和状态机的理解，也能接入 ROS2 的 topic、launch、rosbag、RViz 等生态，后续可以逐步把模块状态、路径、任务和故障事件拆成 typed messages。
