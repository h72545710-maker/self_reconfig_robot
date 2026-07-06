# Self Reconfig Robot Control Prototype

面向自重构机器人的嵌入式 Linux 多模块协同控制与 ROS2 桥接原型系统。

本项目将多个机器人单体抽象为可独立运行的模块，重点验证嵌入式 Linux 上位机控制层能力：合体编队、分体并行探索、动态 leader 选举、故障恢复、低电量让渡、动态障碍重规划、协同感知覆盖、Linux 串口桥接、systemd 服务化部署，以及从 C++ 控制状态到 ROS2 topic 的桥接发布。

## 项目定位

当前版本是控制层和系统软件原型，不是完整机械自重构整机。它适合用于展示：

- Linux C++ 工程能力
- 多模块任务调度和状态机设计
- A* 路径规划与航点平滑
- UDP/ACK/心跳/故障上报通信链路
- 树莓派部署和 ROS2 Jazzy bridge
- Web 可视化演示与复盘
- STM32 下位机 UART 接口预留
- Linux systemd 服务化和健康监控
- pty 虚拟串口联调

## 已实现功能

- `coupled` 合体编队模式：leader 带队，follower 根据队形目标独立规划。
- `split` 分体并行模式：将探索任务拆分给多个模块并行执行。
- 多阶段任务流程：`RENDEZVOUS -> EXPLORE -> RETURN`。
- 动态 leader 选举：综合电量、目标距离和在线状态进行选择。
- 故障恢复：模块故障后重选 leader，故障模块作为动态障碍绕行。
- 低电量让渡：当前 leader 电量过低时触发控制权切换。
- 路径规划：A* 搜索 + line-of-sight 航点平滑。
- 协同感知：模拟融合里程计、视觉定位、IMU 航向和前向距离，统计覆盖率和融合置信度。
- UDP 多进程通信：模块注册、心跳、ACK、任务下发、状态回传、故障上报。
- STM32 bridge：预留 UART 下位机接口，支持 `ODOM/BAT/FAULT` 回传。
- Mock STM32：基于 Linux pty 创建 `/tmp/self_reconfig_stm32` 虚拟串口，便于无硬件联调。
- systemd 部署：支持控制程序开机自启、异常重启、journal 日志和 timer 健康检查。
- ROS2 bridge：将控制状态发布为多个语义 topic。
- Web UI：展示地图、模块状态、任务阶段、路径、队形、事件日志和指标。

## 目录结构

```text
self_reconfig_robot/
  include/robot/          C++ headers
  src/                    C++ simulator, UDP nodes, STM32 bridge
  scripts/                build, deploy, ROS2 Docker scripts
  deploy/systemd/         board-side systemd service/timer units
  web/                    browser visualization
  ros2/                   ROS2 bridge package
  docs/                   design notes, interview/resume material
  CMakeLists.txt
```

## 本地构建

Windows:

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\robot_sim.exe --ticks 60 --sleep-ms 120 --state-file build\control_state.json
```

Linux / Raspberry Pi:

```bash
bash scripts/build_native.sh
./build/robot_sim --ticks 60 --sleep-ms 120 --state-file build/control_state.json
```

## Web 可视化

Windows:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_control_visual.ps1
```

浏览器会打开控制状态页面，实时读取 `build/control_state.json`。

## ROS2 Bridge

在树莓派 Ubuntu 25.10 上，本项目使用 Docker 运行 ROS2 Jazzy 环境：

```bash
bash scripts/install_ros2_jazzy_docker_ubuntu.sh
bash scripts/run_ros2_jazzy_docker.sh
```

进入容器后编译 ROS2 bridge：

```bash
bash scripts/build_ros2_bridge_in_docker.sh
```

启动 bridge：

```bash
source /opt/ros/jazzy/setup.bash
source /workspace/self_reconfig_robot/ros2/install/setup.bash
ros2 launch self_reconfig_control control_bridge.launch.py state_file:=/workspace/self_reconfig_robot/build/control_state.json
```

当前发布的 topic：

```text
/self_reconfig/control_state  std_msgs/msg/String
/self_reconfig/mission        std_msgs/msg/String
/self_reconfig/modules        std_msgs/msg/String
/self_reconfig/path           std_msgs/msg/String
/self_reconfig/metrics        std_msgs/msg/String
/self_reconfig/events         std_msgs/msg/String
```

检查 topic：

```bash
ros2 topic list -t | grep self_reconfig
ros2 topic echo --once /self_reconfig/mission
ros2 topic echo --once /self_reconfig/metrics
```

## 树莓派部署

Windows 同步代码到树莓派：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\deploy_pi.ps1
powershell -ExecutionPolicy Bypass -File .\scripts\deploy_ros2_scripts_pi.ps1
```

默认目标：

```text
yu@192.168.1.119:~/self_reconfig_robot
```

## 嵌入式 Linux 板端能力

串口模拟联调：

```bash
bash scripts/run_linux_serial_demo.sh
```

这个 demo 会启动 `mock_stm32`、`master_node` 和 `stm32_bridge`，通过 `/tmp/self_reconfig_stm32` 模拟真实 STM32 串口回传。

安装 systemd 服务：

```bash
bash scripts/install_systemd_services.sh
sudo systemctl start self-reconfig-robot-sim.service
sudo systemctl start self-reconfig-health.timer
journalctl -u self-reconfig-robot-sim.service -f
```

健康检查：

```bash
bash scripts/health_monitor.sh
```

## 面试表达

一句话版本：

> 基于 C++17 在树莓派 Linux 环境实现自重构机器人多模块协同控制原型，完成 UDP 通信、UART 桥接、systemd 服务化、健康监控和 ROS2 Jazzy 多 topic 状态发布，验证任务调度、路径规划、故障恢复和协同感知过程。

更多简历和面试材料见：

- `docs/resume_insert_self_reconfig_robot.md`
- `docs/resume_project_order_suggestion.md`
- `docs/ros2_lower_fusion_upgrade.md`
- `docs/embedded_linux_upgrade_plan.md`
- `docs/embedded_linux_interview_guide.md`
- `docs/control_design.md`
