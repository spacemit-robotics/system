# sys - 系统控制模块

## 项目简介

系统控制模块，提供系统电源模式管理、看门狗和唤醒源控制功能，供上层应用在休眠、唤醒、看门狗喂狗等场景下使用。

## 功能特性

### 系统模式管理
- 支持多种电源模式：ACTIVE、IDLE、SENSOR_OFF、SUSPEND、HIBERNATE、POWEROFF、REBOOT
- 模式切换钩子函数支持
- 模式状态查询

### 看门狗控制
- 基于 Linux `/dev/watchdog` 设备
- 自动初始化和超时配置
- 定期喂狗功能

### 唤醒源控制
- 网络唤醒（Wake-on-LAN）支持
- GPIO 唤醒源配置
- 支持高电平/低电平触发

### 系统信息获取
- 获取 CPU 温度
- 获取 ADC 通道值
- 获取 NTC 温度

## 快速开始

### 环境准备

- Linux 内核（watchdog、GPIO sysfs、ethtool 支持）
- CMake ≥ 3.16

### 构建编译

```bash
cd components/system/sys
mkdir build && cd build
cmake ..
cmake --build .
```

仅编译库（不编译测试）：

```bash
cmake -DBUILD_SYS_TESTS=OFF ..
cmake --build .
```

## 运行测试

### 运行所有测试

```bash
cd build
./test_sys
```

或使用 CTest：

```bash
cd build
ctest --verbose
```

### 测试说明

测试用例包括：
1. **系统模式操作测试** - 测试模式设置和获取
2. **钩子注册测试** - 测试钩子函数注册和调用
3. **钩子失败处理测试** - 测试钩子函数失败时的处理
4. **看门狗操作测试** - 测试看门狗喂狗功能
5. **网络唤醒源测试** - 测试网络唤醒启用/禁用
6. **GPIO 唤醒源测试** - 测试 GPIO 唤醒配置
7. **所有模式测试** - 遍历测试所有系统模式

**注意：** 某些测试需要 root 权限才能完全通过（如看门狗、网络唤醒、GPIO 操作）。

### 以 root 权限运行测试

```bash
sudo ./test_sys
```

## API 使用示例

### 系统模式控制

```c
#include <sys.h>

// 设置系统模式
sys_set_mode(SYS_MODE_IDLE);

// 获取当前模式
enum sys_mode mode = sys_get_mode();
```

### 注册钩子函数

```c
int my_hook(enum sys_mode target_mode, void *ctx) {
    printf("Mode changing to: %d\n", target_mode);
    return 0;  // 返回 0 表示成功
}

void *context = "my context";
sys_register_hook(my_hook, context);
```

### 看门狗控制

```c
// 喂狗（重置看门狗定时器）
sys_watchdog_feed();
```

### 网络唤醒控制

```c
// 启用网络唤醒
sys_enable_wakeup_net(true);

// 禁用网络唤醒
sys_enable_wakeup_net(false);
```

### GPIO 唤醒控制

```c
// 启用 GPIO 17，低电平触发
sys_enable_wakeup_gpio(17, true);

// 启用 GPIO 27，高电平触发
sys_enable_wakeup_gpio(27, false);
```

### 系统信息接口

```c
int sys_get_cpu_temperature(float *temp);
int sys_get_adc_value(int channel, int *value);
int sys_get_ntc_temperature(int channel, float *temp);
```

```c
float cpu_temp = 0.0f;
float ntc_temp = 0.0f;
int adc_value = 0;

sys_get_cpu_temperature(&cpu_temp);
sys_get_adc_value(1, &adc_value);//具体的通道，需要根据实际硬件来定
sys_get_ntc_temperature(1, &ntc_temp);
```

## 配置选项

可以通过修改 `src/sys.c` 中的宏定义来配置：

```c
#define MAX_SYS_HOOKS 100             // 最大钩子数量
#define WATCHDOG_DEVICE "/dev/watchdog"  // 看门狗设备路径
#define WATCHDOG_TIMEOUT 30           // 看门狗超时时间（秒）
#define DEFAULT_NET_DEVICE "eth0"     // 默认网络设备
```

## 依赖

- Linux 内核 watchdog 支持
- Linux GPIO sysfs 接口
- ethtool 支持（用于 WoL）

## License

本组件源码文件头声明为 Apache-2.0，最终以上级目录 `LICENSE` 文件为准。
