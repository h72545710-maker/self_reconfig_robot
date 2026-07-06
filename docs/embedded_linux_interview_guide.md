# 嵌入式 Linux 面试准备指南

## Linux 岗位一般会问什么

嵌入式 Linux 软件开发通常不会只问会不会敲命令，而是看你是否理解“程序在 Linux 板子上怎么稳定运行、怎么和硬件通信、怎么定位问题”。

常见问题可以分成 8 类。

## 1. Linux 基础命令和排障

常见问题：

- 怎么查看进程？
- 怎么查看端口占用？
- 怎么看日志？
- 怎么查看 CPU、内存、磁盘？
- 程序跑不起来怎么定位？

项目回答：

- 用 `ps` / `pgrep` 检查 `robot_sim`、`stm32_bridge` 是否运行。
- 用 `journalctl -u self-reconfig-robot-sim.service -f` 查看 systemd 服务日志。
- 用 `run_logs/*.log` 保存串口桥接、master 节点、mock STM32 日志。
- 用 `scripts/health_monitor.sh` 检查进程和状态文件心跳。

可以说：

> 我在项目里把控制程序服务化部署到了树莓派上，通过 systemd 管理进程，用 journalctl 和自定义健康检查脚本定位运行问题。

## 2. 进程、线程和 IPC

常见问题：

- 进程和线程区别？
- 多进程之间怎么通信？
- 什么是阻塞和非阻塞 IO？
- 怎么避免一个模块卡死影响整体系统？

项目回答：

- `master_node`、`module_node`、`stm32_bridge`、`mock_stm32` 是多个独立进程。
- 模块之间通过 UDP 自定义协议通信。
- 串口使用非阻塞读写，避免下位机没有数据时卡住上位机。
- heartbeat 超时后 master 会标记模块 offline 并重新选 leader。

可以说：

> 我把不同模块拆成独立进程，通过 UDP 心跳和 ACK 做状态同步，避免单个模块异常影响整个控制流程。

## 3. Linux 网络编程

常见问题：

- TCP 和 UDP 区别？
- UDP 为什么可能丢包？
- UDP 怎么做可靠性？
- socket bind/send/recv 大概流程？

项目回答：

- 本项目用 UDP 模拟多模块状态上报。
- 每个消息包含 `magic/length/type/module_id/sequence/payload/CRC`。
- master 下发角色和任务后等待 ACK，超时重传。
- 心跳超时用于检测模块掉线。

可以说：

> UDP 本身不保证可靠，所以我在应用层加了 sequence、ACK 和重传机制；心跳用于在线状态检测，CRC 用于帧校验。

## 4. 串口和设备文件

常见问题：

- Linux 下串口设备一般在哪里？
- `termios` 配置过吗？
- 什么是 `/dev/ttyUSB0`、`/dev/ttyACM0`？
- 怎么调试没有真实硬件的串口程序？

项目回答：

- `serial_port.cpp` 使用 `termios` 配置 115200、8N1、非阻塞模式。
- `stm32_bridge` 可以连接 `/dev/ttyUSB0` 或 `/tmp/self_reconfig_stm32`。
- `mock_stm32` 使用 Linux pty 创建虚拟串口，模拟 STM32 回传。

可以说：

> 为了在没有真实 STM32 的情况下验证链路，我用 Linux pty 模拟了一个串口设备，并让 bridge 按真实串口方式读取 `ODOM/BAT/FAULT`。

## 5. systemd 和板端部署

常见问题：

- 程序怎么开机自启？
- 崩溃后怎么自动拉起？
- 怎么查看服务状态？
- systemd service 文件里常见字段有哪些？

项目回答：

- `deploy/systemd/self-reconfig-robot-sim.service` 管理控制仿真。
- `Restart=on-failure` 支持异常退出自动重启。
- `self-reconfig-health.timer` 定期执行健康检查。

可以说：

> 我把控制程序做成 systemd service，配置了 WorkingDirectory、ExecStart、Restart 和日志输出，并用 timer 定期检查状态文件是否更新。

## 6. 文件、日志和状态监控

常见问题：

- 怎么判断程序还活着？
- 只看进程存在够不够？
- 日志怎么保存？
- 状态文件多久没更新算异常？

项目回答：

- `health_monitor.sh` 不只检查进程，还检查 `control_state.json` 更新时间。
- `run_logs/` 保存 demo 日志。
- systemd 版本用 journal 统一管理日志。

可以说：

> 只检查进程不够，因为进程可能卡死但不退出。所以我额外检查状态文件更新时间，把它当作控制循环心跳。

## 7. ROS2 基础

常见问题：

- ROS2 node/topic 是什么？
- launch 文件干什么？
- ROS2 和普通 socket 有什么区别？
- 你项目里 ROS2 做到了哪一步？

项目回答：

- C++ 控制核心仍独立运行。
- ROS2 bridge 读取 `control_state.json`。
- 发布多个 topic：mission、modules、path、metrics、events。
- 当前用 `std_msgs/String` 承载 JSON 片段，后续可以升级 typed messages。

可以说：

> 我没有一开始就把控制逻辑强行写进 ROS2，而是先保持 C++ 控制核心独立，再通过 bridge 接入 ROS2 topic 生态，这样便于调试和逐步迁移。

## 8. C++ 工程能力

常见问题：

- C++ 项目怎么组织？
- CMake 用过吗？
- 如何避免模块耦合？
- 怎么做跨平台？

项目回答：

- `include/robot` 放公共类型、协议、地图、串口封装。
- `src/` 分为控制仿真、master、module、bridge、mock 下位机。
- Windows 用 CMake/MSVC 构建，Linux 用 `build_native.sh` 或 CMake 构建。
- 串口和 mock 下位机只在 Linux 启用。

可以说：

> 我按模块拆分了协议、地图、串口、UDP、控制仿真和 ROS2 bridge，Windows 侧便于开发和可视化，树莓派侧用于 Linux 部署验证。

## 你应该重点掌握的命令

```bash
ps aux | grep robot
pgrep -af robot_sim
journalctl -u self-reconfig-robot-sim.service -f
systemctl status self-reconfig-robot-sim.service
systemctl list-timers
ls -l /dev/ttyUSB* /dev/ttyACM* /tmp/self_reconfig_stm32
stty -F /dev/ttyUSB0 -a
ss -lunp
tail -f run_logs/stm32_bridge.log
ros2 topic list -t
ros2 topic echo --once /self_reconfig/mission
```

## 面试时别说太满

不要说：

> 我已经完整掌握嵌入式 Linux。

建议说：

> 我之前偏 STM32 和上位机项目多一些，最近在补嵌入式 Linux 和 ROS2。我把这个项目部署到树莓派上，做了 UDP 通信、串口桥接、systemd 服务化、健康监控和 ROS2 topic 发布，用这个项目把 Linux 板端开发流程跑了一遍。

这个说法真实，也更容易让面试官继续问你项目细节。

