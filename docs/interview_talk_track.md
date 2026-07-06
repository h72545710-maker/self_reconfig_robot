# 面试讲解稿：自重构机器人多模块协同控制系统

## 1. 项目一句话介绍

这个项目是一个面向自重构救援机器人的多模块协同控制原型。我使用树莓派作为嵌入式 Linux 主控节点，用 C++ 实现模块通信、状态同步、leader 选举、任务调度、故障恢复和 Web 可视化。

## 2. 为什么做这个项目

自重构机器人由多个单体模块组成，真实场景下会遇到模块掉线、故障、通信丢包和任务中断等问题。相比单体机器人，它更需要一个可靠的软件控制层来管理模块状态、分配任务，并在某个模块异常时自动恢复。

所以我没有直接从机械对接或完整 SLAM 开始，而是先实现自重构机器人底层必须具备的多模块通信与调度框架。

## 3. 系统架构

系统分为三部分：

- `master_node`：运行在树莓派或 PC 上，负责模块注册、心跳检测、leader 选举、任务分配和故障恢复。
- `module_node`：模拟单个机器人模块，负责注册、心跳上报、接收角色/任务命令、回 ACK、上报任务进度。
- Web 可视化：读取 `state.json`，展示模块地图、任务进度和事件日志。

通信方式使用 UDP，自定义二进制协议，后续可以替换成 UART、CAN 或真实无线模块。

## 4. 通信协议怎么设计

我设计了一个固定格式的二进制协议帧：

```text
magic | length | type | module_id | sequence | payload | crc16
```

其中：

- `magic` 用于识别帧头。
- `length` 表示整帧长度。
- `type` 区分 REGISTER、HEARTBEAT、ROLE、ACK、FAULT、TASK_ASSIGN、TASK_STATUS。
- `module_id` 标识模块。
- `sequence` 用于追踪消息和 ACK。
- `payload` 存放不同消息的业务数据。
- `crc16` 用 CRC16-CCITT 校验错误帧。

为了保证跨平台兼容，我没有直接发送结构体内存，而是手动按小端序编码字段，避免 Windows 和 ARM Linux 上结构体对齐差异。

## 5. 心跳和故障恢复怎么做

模块启动后会向 master 注册，并周期性发送 heartbeat。master 记录每个模块的 `last_seen` 时间。

异常分两类：

- 主动故障：模块发送 FAULT，master 标记为 `fault`。
- 静默离线：模块不再发送心跳，master 超时后标记为 `offline`。

如果当前 leader 变成 fault 或 offline，master 会根据在线模块重新选举 leader，并下发新的 ROLE 命令。

## 6. ACK 和重传怎么做

对关键控制命令，例如 ROLE 和 TASK_ASSIGN，master 会维护待确认状态：

- 发送命令时记录 sequence。
- module 收到命令后返回 ACK，ACK payload 携带被确认的 sequence。
- master 收到匹配 ACK 后清除 pending 状态。
- 如果 600 ms 内没有收到 ACK，master 重发命令，最多重试 5 次。

我专门做了一个演示，让 module 故意丢弃第一次 ACK，可以看到 master 打印 retry，然后第二次收到 ACK。

## 7. 任务调度怎么做

目前实现了一个最小任务模型 `EXPLORE`。

流程是：

1. master 创建任务。
2. master 将任务分配给当前 leader。
3. leader ACK 任务并开始上报 `running/progress`。
4. 如果 leader 离线，master 重新选举 leader。
5. master 将同一个任务重新分配给新 leader。
6. 新 leader 继续上报任务进度。

这个功能体现的是多模块系统的容错调度能力。

## 8. 和自重构机器人的关系

自重构机器人不只是机械结构能拼接，还需要软件上知道当前有哪些模块、每个模块状态如何、谁负责主控、任务如何分配、某个模块损坏后如何恢复。

我这个项目实现的是自重构机器人中间的软件控制层：

```text
感知/路径规划/重构策略
        ↑
通信协议 + 状态同步 + leader 选举 + 任务调度 + 故障恢复
        ↑
底盘控制/电机/传感器/对接机构
```

后续可以接入真实 STM32 底盘、IMU、超声波、编码器或 CAN/UART 通信。

## 9. 如果面试官问为什么不用 ROS2

可以这样回答：

这个项目早期我希望先验证底层通信协议、可靠命令下发、心跳检测和故障恢复机制，所以先用 C++ 和 UDP 自己实现了一个轻量控制框架。这样可以更清楚地掌握模块间通信和异常处理细节。后续如果接入 ROS2，可以把 `module_node` 和 `master_node` 映射成 ROS2 node，把任务分配迁移成 action，把状态同步迁移成 topic。

## 10. 项目还可以怎么扩展

- 接入真实树莓派 + STM32 小车。
- 使用 UART/CAN 替代本地 UDP。
- 引入 A* 或 Nav2 路径规划。
- 增加任务队列和任务优先级。
- 增加模块拓扑关系和对接状态。
- 增加日志持久化和历史回放。

