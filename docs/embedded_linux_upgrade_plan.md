# 嵌入式 Linux 方向升级说明

## 为什么要升级

原项目重点是机器人协同控制、路径规划、ROS2 bridge 和 Web 可视化。它和嵌入式 Linux 有关系，但还不够强。

为了让项目更贴近嵌入式 Linux 软件开发岗位，本次补充了板端工程能力：

- Linux 串口设备模拟与 `termios` 通信验证
- 树莓派原生构建和运行脚本
- systemd 服务化部署
- 进程健康监控
- journalctl 日志查看
- ROS2 Docker bridge 服务化预留

这样项目可以从“机器人控制仿真”升级为：

> 基于嵌入式 Linux 的自重构机器人上位机控制与 ROS2 桥接系统

## 新增内容

### 1. 虚拟 STM32 串口设备

新增：

```text
src/mock_stm32.cpp
```

它在 Linux 上创建伪终端设备，并生成软链接：

```text
/tmp/self_reconfig_stm32 -> /dev/pts/x
```

`stm32_bridge` 可以像连接真实 `/dev/ttyUSB0` 一样连接它：

```bash
./build/stm32_bridge --serial /tmp/self_reconfig_stm32
```

支持的下位机回传：

```text
PONG
ACK VEL
ODOM <x> <y> <yaw> <battery>
FAULT MOTOR_STALL
```

这部分可以体现：

- Linux 设备文件
- 伪终端 pty
- UART 协议模拟
- 上下位机解耦调试
- 串口非阻塞读写

### 2. 串口联调 Demo

新增：

```text
scripts/run_linux_serial_demo.sh
```

运行：

```bash
bash scripts/run_linux_serial_demo.sh
```

它会启动：

```text
mock_stm32 -> /tmp/self_reconfig_stm32
master_node
stm32_bridge
```

并写入日志：

```text
run_logs/mock_stm32.log
run_logs/master_node.log
run_logs/stm32_bridge.log
```

面试可以说：

> 我用 Linux pty 模拟真实串口设备，在没有 STM32 板子的情况下先验证上位机串口读写、协议解析和故障上报链路。

### 3. systemd 服务化部署

新增：

```text
deploy/systemd/self-reconfig-robot-sim.service
deploy/systemd/self-reconfig-mock-stm32.service
deploy/systemd/self-reconfig-ros2-bridge.service
deploy/systemd/self-reconfig-health.service
deploy/systemd/self-reconfig-health.timer
```

安装：

```bash
bash scripts/install_systemd_services.sh
```

常用命令：

```bash
sudo systemctl start self-reconfig-robot-sim.service
sudo systemctl status self-reconfig-robot-sim.service
journalctl -u self-reconfig-robot-sim.service -f
sudo systemctl start self-reconfig-health.timer
systemctl list-timers | grep self-reconfig
```

这部分可以体现：

- Linux 服务化部署
- 开机自启
- 异常退出自动重启
- journal 日志
- timer 定期任务

### 4. 健康监控脚本

新增：

```text
scripts/health_monitor.sh
```

运行：

```bash
bash scripts/health_monitor.sh
```

检查内容：

- `robot_sim` 进程是否运行
- `control_state.json` 是否存在
- 状态文件是否在最近几秒更新
- 串口设备 `/tmp/self_reconfig_stm32` 是否存在

面试可以说：

> 我没有只让程序跑起来，还做了板端健康检查：检查进程存活、状态文件心跳和串口设备是否存在，用 systemd timer 定期执行，便于现场部署和排障。

## 嵌入式 Linux 岗位相关性

这个项目现在能对应到嵌入式 Linux 常见能力：

| 岗位能力 | 项目体现 |
| --- | --- |
| C/C++ 编程 | C++17 控制核心、协议编解码、路径规划 |
| Linux 进程 | master、module、bridge、mock_stm32 多进程协作 |
| Linux IO | 串口 `termios`、非阻塞读写、伪终端 pty |
| 网络通信 | UDP 注册、心跳、ACK、任务下发 |
| 服务化部署 | systemd service、timer、自动重启 |
| 日志排障 | run_logs、journalctl |
| 板端部署 | 树莓派 Ubuntu、Docker、ROS2 Jazzy |
| 上下位机架构 | 树莓派上位机 + STM32 下位机接口 |
| 机器人软件 | ROS2 bridge、多 topic 状态发布 |

## 简历表达升级

原表达：

> 自重构机器人多模块协同控制与 ROS2 桥接系统

升级后：

> 基于嵌入式 Linux 的自重构机器人多模块协同控制与 ROS2 桥接系统

推荐 bullet：

- 基于 C++17 在树莓派 Linux 环境实现多模块机器人上位机控制原型，支持合体编队、分体并行、动态 leader 选举、故障恢复和动态障碍重规划。
- 基于 UDP 设计模块注册、心跳检测、ACK 确认、任务下发和故障上报链路，实现多进程模块状态同步。
- 基于 Linux `termios` 实现 UART 桥接层，预留 STM32 下位机接入接口，并通过 pty 虚拟串口模拟 `ODOM/BAT/FAULT` 回传完成板端联调验证。
- 使用 systemd 将控制程序服务化部署到树莓派，支持开机自启、异常退出自动重启、journalctl 日志查看和 timer 定期健康检查。
- 在 Docker 中部署 ROS2 Jazzy，开发 bridge 节点将控制状态拆分发布为 mission、modules、path、metrics、events 等 ROS2 topic。

