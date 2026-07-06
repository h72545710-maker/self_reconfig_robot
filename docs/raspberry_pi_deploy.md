# 树莓派部署说明

目标板：

```text
yu@192.168.1.119
```

## 1. 在 Windows 端同步并远程构建

在 PowerShell 中进入项目目录：

```powershell
cd E:\LINUX\面试\self_reconfig_robot
```

只同步、编译：

```powershell
.\scripts\deploy_pi.ps1
```

同步、编译并运行：

```powershell
.\scripts\deploy_pi.ps1 -Run
```

同步、编译并直接运行任务重分配演示：

```powershell
.\scripts\deploy_pi.ps1 -RunTaskDemo
```

也可以选择其他演示：

```powershell
.\scripts\deploy_pi.ps1 -RunUdpDemo
.\scripts\deploy_pi.ps1 -RunOfflineDemo
.\scripts\deploy_pi.ps1 -RunAckDemo
```

如果后续 IP 或用户名变化：

```powershell
.\scripts\deploy_pi.ps1 -Target "yu@192.168.1.119" -Run
```

如果没有配置 SSH 免密登录，运行脚本时会提示输入 `yu` 用户密码，这是正常的。

## 2. 在树莓派本地编译运行

登录树莓派：

```bash
ssh yu@192.168.1.119
```

进入项目目录：

```bash
cd ~/self_reconfig_robot
```

构建运行：

```bash
bash scripts/build_native.sh
./build/robot_sim
```

也可以使用脚本：

```bash
bash scripts/pi_build_run.sh
```

运行 UDP 多进程通信演示：

```bash
bash scripts/run_udp_demo.sh
```

你会看到 `master_node` 收到多个 `module_node` 的注册和心跳。默认演示会让 1 号模块先成为 leader，然后模拟 1 号模块故障，系统自动切换 2 号模块为新的 leader。

输出中的 `seq` 是模块发送帧的序列号，`crc=0x....` 表示该注册帧通过了 CRC16-CCITT 校验。

运行心跳超时离线演示：

```bash
bash scripts/run_offline_demo.sh
```

这个演示中，1 号模块不会主动上报故障，而是在运行 3 秒后静默停止心跳。master 超时后会将 1 号模块标记为 `offline`，并切换 2 号模块为新的 leader。

运行 ACK 丢失与超时重传演示：

```bash
bash scripts/run_ack_demo.sh
```

这个演示中，1 号模块会故意丢弃第一次 ROLE 命令 ACK。master 超时后会重发同一条 ROLE 命令，随后收到模块 ACK。输出中可以看到 `[send]`、`[retry]` 和 `[ack]`。

运行任务重分配演示：

```bash
bash scripts/run_task_demo.sh
```

这个演示中，master 会创建 `EXPLORE` 任务并分配给当前 leader。1 号 leader 静默离线后，master 会将任务重新分配给 2 号 leader。输出中可以看到 `[task-assign]`、`[task-status]`、`[task-reassign]` 和 `[task-ack]`。

注意：这些 demo 默认都使用 UDP 9000 端口，一次只运行一个 demo。

## 3. 本地 Web 可视化

Windows 本地可视化演示：

```powershell
cd E:\LINUX\面试\self_reconfig_robot
powershell -ExecutionPolicy Bypass -File .\scripts\run_visual_demo.ps1
```

脚本会自动打开：

```text
http://127.0.0.1:8080/web/index.html
```

页面读取 `build/state.json`，展示模块位置、leader、offline/fault、任务进度和事件日志。

## 4. 树莓派需要的软件包

如果树莓派没有 g++：

```bash
sudo apt update
sudo apt install -y build-essential
```

当前树莓派部署脚本默认使用 `scripts/build_native.sh` 直接调用 `g++` 构建，避免部分系统上 `cmake` 异常崩溃影响演示。

如果希望后续部署不再反复输入密码，可以在 Windows PowerShell 中生成并拷贝 SSH 公钥：

```powershell
ssh-keygen
type $env:USERPROFILE\.ssh\id_rsa.pub | ssh yu@192.168.1.119 "mkdir -p ~/.ssh && cat >> ~/.ssh/authorized_keys"
```

## 5. 项目展示口径

这个版本可以说成：

项目在树莓派 Linux 环境下部署运行，将树莓派作为机器人上位控制节点，使用 C++ 实现多模块状态管理、路径规划、leader-follower 协同控制和故障恢复仿真。后续可通过 UART/CAN 接入 STM32 运动控制板，实现真实底盘闭环控制。

## 6. 下一步建议

树莓派已经具备 Linux 环境，下一阶段建议把当前单进程仿真拆分为：

- `master_node`：运行在树莓派上，负责模块注册、心跳、任务调度、路径规划。
- `module_node`：模拟或运行在各个机器人模块上，负责状态上报和控制执行。

当前已经完成 UDP 版本的 `master_node` 和 `module_node`，并支持自定义二进制协议帧、CRC16-CCITT 校验、ROLE 命令 ACK/重传、主动故障上报、心跳超时离线检测、EXPLORE 任务分配与任务重分配，并提供本地 Web 可视化。下一步可以继续做 HEARTBEAT 丢包统计、任务命令队列，再接 UART/CAN。
