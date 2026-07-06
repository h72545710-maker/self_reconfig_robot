# Hardware Integration Plan

目标：把当前仿真里的一个 `module_node` 替换成真实硬件模块。树莓派继续承担嵌入式 Linux 上位机职责，STM32H750 承担实时电机控制职责。

## 当前硬件基础

- STM32 工程路径：`E:\STM\FREERTOS\robot`
- 串口：USART1，115200，8N1，PA9/PA10
- 电机驱动：TB6612 风格控制
- PWM：TIM3 CH1/CH2，PA6/PA7，占空比范围 0 到 999
- 方向控制：PC4/PC5 控制左电机，PB0/PB1 控制右电机，PE7 为 STBY
- 已有命令：`PING`、`STOP`、`VEL <left> <right>`

## 新增软件边界

`stm32_bridge` 运行在树莓派上：

- 向 `master_node` 注册为一个真实模块。
- 周期性发送 heartbeat/status。
- 接收 master 的角色命令和任务命令。
- 通过串口把任务转换成 STM32 能理解的 `VEL/STOP/PING`。
- 从 STM32 读取回包并打印，便于调试链路。

这样项目可以形成真实闭环：

`Web UI -> master_node -> UDP protocol -> stm32_bridge -> UART -> STM32H750 -> PWM/TB6612 -> motor`

## 树莓派运行方式

先部署并编译：

```bash
bash scripts/build_native.sh
```

启动 master：

```bash
./build/master_node --state-file build/state.json
```

启动真实 STM32 桥接模块：

```bash
./build/stm32_bridge --id 1 --master 127.0.0.1 --port 9000 --serial /dev/ttyUSB0 --baud 115200 --x 2 --y 1 --battery 95 --drive-pwm 260
```

如果串口设备不是 `/dev/ttyUSB0`，常见替代是：

```bash
ls /dev/ttyUSB* /dev/ttyACM* /dev/serial/by-id/* 2>/dev/null
```

## 安全测试顺序

1. 先不接电机电源，只接 USB-TTL，确认能看到 `[stm32-rx] PONG`。
2. 架空轮子，低速运行 `--drive-pwm 180`。
3. 确认 `TaskAssign` 后电机会转，任务完成后会收到 `STOP`。
4. 再接入真实机构，逐步提高 PWM。

## 秋招表达重点

这一步的意义不是“做了一个网页”，而是把系统拆成了上位机和下位机：

- 树莓派侧：Linux C++、UDP 通信、状态机、任务分配、故障检测、可视化。
- STM32 侧：UART 中断收包、PWM 电机控制、实时执行。
- 系统侧：从任务下发、ACK、执行状态回传到故障重分配，形成可演示闭环。
