# 系统模式实现详解

本文档详细说明了每种系统电源模式的内部实现。

## 模式概览

| 模式 | CPU | 屏幕 | 网络 | 传感器 | 说明 |
|------|-----|------|------|--------|------|
| ACTIVE | 性能模式 | 全亮 | 开启 | 开启 | 全速运行 |
| IDLE | 省电模式 | 关闭 | 开启 | 开启 | CPU降频，屏幕关闭 |
| SENSOR_OFF | 省电模式 | 关闭 | 开启 | 关闭 | 传感器关闭 |
| SUSPEND | 挂起 | 关闭 | 关闭 | 关闭 | 挂起到内存 |
| HIBERNATE | 休眠 | 关闭 | 关闭 | 关闭 | 挂起到磁盘 |
| POWEROFF | - | - | - | - | 关机 |
| REBOOT | - | - | - | - | 重启 |

## 内部实现函数

### 1. SYS_MODE_ACTIVE - 活动模式

**函数：** `mode_active()`

**操作：**
- 设置 CPU 频率调节器为 `performance`（性能模式）
- 设置屏幕亮度为 100%
- 启用网络接口
- 喂看门狗

**实现细节：**
```c
static int mode_active(void)
{
    set_cpu_governor("performance");  // 最高性能
    set_backlight(100);                // 全亮
    control_network(true);             // 网络开启
    sys_watchdog_feed();               // 喂狗
    return 0;
}
```

**系统调用：**
- `/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor` ← "performance"
- `/sys/class/backlight/backlight/brightness` ← 100
- `ip link set eth0 up`

---

### 2. SYS_MODE_IDLE - 空闲模式

**函数：** `mode_idle()`

**操作：**
- 设置 CPU 频率调节器为 `powersave`（省电模式）
- 关闭屏幕（亮度设为 0）
- 保持网络开启
- 喂看门狗

**实现细节：**
```c
static int mode_idle(void)
{
    set_cpu_governor("powersave");     // 省电模式
    set_backlight(0);                  // 关闭屏幕
    control_network(true);             // 保持网络
    sys_watchdog_feed();               // 喂狗
    return 0;
}
```

**系统调用：**
- `/sys/devices/system/cpu/cpu*/cpufreq/scaling_governor` ← "powersave"
- `/sys/class/backlight/backlight/brightness` ← 0
- 网络保持开启

**使用场景：**
- 用户暂时不使用设备
- 需要保持网络连接（如接收消息）
- 快速唤醒

---

### 3. SYS_MODE_SENSOR_OFF - 传感器关闭模式

**函数：** `mode_sensor_off()`

**操作：**
- 设置 CPU 为省电模式
- 关闭屏幕
- 保持网络开启
- 关闭所有传感器（IMU、摄像头等）
- 喂看门狗

**实现细节：**
```c
static int mode_sensor_off(void)
{
    set_cpu_governor("powersave");
    set_backlight(0);
    control_network(true);
    // TODO: 禁用传感器接口
    sys_watchdog_feed();
    return 0;
}
```

**使用场景：**
- 不需要传感器数据
- 进一步降低功耗
- 保持网络通信能力

---

### 4. SYS_MODE_SUSPEND - 挂起模式（STR）

**函数：** `mode_suspend()`

**操作：**
- 关闭网络接口
- 挂起到内存（Suspend to RAM）
- 系统进入低功耗状态

**实现细节：**
```c
static int mode_suspend(void)
{
    control_network(false);            // 关闭网络
    return suspend_to_ram();           // 挂起到内存
}

static int suspend_to_ram(void)
{
    int fd = open("/sys/power/state", O_WRONLY);
    write(fd, "mem", 3);
    close(fd);
    return 0;
}
```

**系统调用：**
- `ip link set eth0 down`
- `/sys/power/state` ← "mem"

**特点：**
- 内存保持供电
- 唤醒速度快（秒级）
- 功耗极低
- 需要配置唤醒源

---

### 5. SYS_MODE_HIBERNATE - 休眠模式（STD）

**函数：** `mode_hibernate()`

**操作：**
- 关闭网络接口
- 挂起到磁盘（Suspend to Disk）
- 将内存内容写入交换分区
- 完全断电

**实现细节：**
```c
static int mode_hibernate(void)
{
    control_network(false);
    return suspend_to_disk();
}

static int suspend_to_disk(void)
{
    int fd = open("/sys/power/state", O_WRONLY);
    write(fd, "disk", 4);
    close(fd);
    return 0;
}
```

**系统调用：**
- `ip link set eth0 down`
- `/sys/power/state` ← "disk"

**特点：**
- 完全断电，零功耗
- 唤醒速度较慢（需要从磁盘恢复）
- 需要交换分区支持
- 适合长时间不使用

---

### 6. SYS_MODE_POWEROFF - 关机模式

**函数：** `mode_poweroff()`

**操作：**
- 关闭网络
- 执行系统关机命令

**实现细节：**
```c
static int mode_poweroff(void)
{
    control_network(false);
    system("poweroff");
    return -1;  // 不会返回
}
```

**系统调用：**
- `ip link set eth0 down`
- `/sys/power/state` ← "poweroff" 或 `poweroff` 命令

---

### 7. SYS_MODE_REBOOT - 重启模式

**函数：** `mode_reboot()`

**操作：**
- 关闭网络
- 执行系统重启命令

**实现细节：**
```c
static int mode_reboot(void)
{
    control_network(false);
    system("reboot");
    return -1;  // 不会返回
}
```

**系统调用：**
- `ip link set eth0 down`
- `reboot` 命令

---

## 辅助函数

### set_cpu_governor()
设置 CPU 频率调节策略。

**支持的调节器：**
- `performance` - 最高性能，固定最高频率
- `powersave` - 省电模式，固定最低频率
- `ondemand` - 按需调节（默认）
- `conservative` - 保守调节
- `schedutil` - 调度器驱动

### set_backlight()
控制屏幕背光亮度（0-100）。

### control_network()
启用或禁用网络接口。

### suspend_to_ram()
执行挂起到内存操作。

### suspend_to_disk()
执行挂起到磁盘操作。

---

## 模式切换流程

```
用户调用 sys_set_mode(mode)
    ↓
验证模式有效性
    ↓
调用所有注册的钩子函数
    ↓
执行 execute_mode_operations(mode)
    ↓
根据模式调用对应的 mode_xxx() 函数
    ↓
更新 current_mode
    ↓
返回结果
```

## 权限要求

大多数模式切换操作需要 **root 权限**：

- CPU 频率调节：需要写入 `/sys/devices/system/cpu/`
- 屏幕亮度控制：需要写入 `/sys/class/backlight/`
- 网络控制：需要 `CAP_NET_ADMIN` 权限
- 挂起/休眠：需要写入 `/sys/power/state`
- 关机/重启：需要 root 权限

## 错误处理

所有内部函数都包含错误处理：
- 文件打开失败时输出错误信息
- 非关键操作失败时继续执行
- 关键操作失败时返回错误码

## 扩展建议

1. **传感器控制**：在 `mode_sensor_off()` 中添加具体的传感器关闭逻辑
2. **CPU 核心控制**：在省电模式下可以关闭部分 CPU 核心
3. **外设控制**：添加 USB、蓝牙等外设的电源管理
4. **唤醒源配置**：在挂起前自动配置唤醒源
5. **状态保存**：在模式切换前保存应用状态

## 测试建议

```bash
# 测试 ACTIVE 模式
sudo ./test_sys

# 测试 IDLE 模式（观察 CPU 频率变化）
watch -n 1 cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq

# 测试 SUSPEND 模式（需要配置唤醒源）
sudo sys_enable_wakeup_gpio(17, true)
sudo sys_set_mode(SYS_MODE_SUSPEND)
```
