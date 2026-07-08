# 验证清单

这份清单用于秋招前自检，也可以在面试时作为“我不是只做了演示页面”的证据。

## 1. 本地构建

```powershell
cmake -S . -B build
cmake --build build --config Release
```

期望结果：

- 生成 `build/Release/robot_sim.exe`
- 生成 `build/Release/master_node.exe`
- 生成 `build/Release/module_node.exe`
- 生成 `build/Release/robot_core_tests.exe`

## 2. 单元测试

```powershell
ctest --test-dir build -C Release --output-on-failure
```

当前覆盖点：

- 协议帧 encode/decode
- CRC 损坏帧拒收
- 任务下发和任务状态 payload 编解码
- A* 绕障规划和动态占用格阻塞
- 传感融合输出范围
- JSON 字符串转义

## 3. 控制仿真

```powershell
.\build\Release\robot_sim.exe --ticks 60 --sleep-ms 0 --state-file build\control_state.json
```

重点观察：

- T8 左右 1 号模块故障
- leader 从 1 号切换到其他在线模块
- 任务从 `RENDEZVOUS` 进入 `EXPLORE`
- 分体子任务逐步完成
- 动态障碍触发重规划
- 低电量触发健康让渡
- 最终进入 `RETURN` 或任务完成

## 4. Web 可视化

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_control_visual.ps1
```

浏览器页面：

```text
http://127.0.0.1:8081/web/index.html?source=control_state.json
```

建议截图：

- 故障模块显示为红色
- leader 切换指标
- 合体/分体/返航时间轴
- 分体子任务目标
- 覆盖率、重规划次数、恢复 tick

## 5. UDP ACK 与重传

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_ack_demo.ps1 -Duration 6
```

期望日志：

- 模块注册成功
- master 下发 leader/follower 角色
- module 故意丢一次 ACK
- master 输出 `[retry]`
- 后续收到 `[ack]`
- leader 收到任务并上报 task progress

## 6. UDP 故障恢复

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\run_udp_demo.ps1 -Duration 8
```

期望日志：

- module 1 上报故障
- master 标记 module 1 为 fault
- leader 重新选举
- 任务重新分配给新的 leader

## 7. Linux 串口桥接

在 Linux / 树莓派环境：

```bash
bash scripts/run_linux_serial_demo.sh
```

期望结果：

- `mock_stm32` 创建 `/tmp/self_reconfig_stm32`
- `stm32_bridge` 通过串口发送 `PING`、`VEL`、`STOP`
- mock STM32 回传 `ODOM`、`BAT` 或 `FAULT`
- master 收到 bridge 转发的心跳、任务状态或故障上报

## 8. ROS2 Bridge

在 ROS2 Jazzy 环境：

```bash
bash scripts/build_ros2_bridge_in_docker.sh
ros2 launch self_reconfig_control control_bridge.launch.py state_file:=/workspace/self_reconfig_robot/build/control_state.json
ros2 topic list -t | grep self_reconfig
```

期望 topic：

```text
/self_reconfig/control_state
/self_reconfig/mission
/self_reconfig/modules
/self_reconfig/path
/self_reconfig/metrics
/self_reconfig/events
```

## 面试建议

如果时间有限，优先演示这三项：

1. `ctest` 通过，证明核心逻辑可验证。
2. Web 页面展示故障恢复和合体/分体任务。
3. `run_ack_demo.ps1` 展示 ACK 重传链路。
