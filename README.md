# components/system

## 项目简介

系统底层支撑组件，提供 DMA 拷贝、共享内存传输、系统电源/看门狗/唤醒控制、OTA 升级等能力，供上层应用和中间件调用。

## 功能特性

| 子模块 | 功能 | 说明 |
|--------|------|------|
| **dma** | DMA 拷贝库 | 基于 `/dev/me_dma` 内核驱动，支持 dma-buf 同步/异步拷贝 |
| **dma/ko** | me_dma 内核驱动 | 提供 DMA 字符设备，供 libdma 调用 |
| **shm** | 共享内存库 | DMA-HEAP 分配、跨进程 fd 传输（shm_transport） |
| **sys** | 系统控制 | 电源模式、看门狗、网络/GPIO 唤醒源 |
| **ota** | OTA 升级 | 本地/HTTP 升级框架（可扩展驱动） |

## 快速开始

### 环境准备

- Linux 内核（支持 DMA-HEAP、watchdog、GPIO sysfs）
- CMake ≥ 3.16
- 编译工具链（gcc/clang）

### 构建编译

各子模块可独立编译：

```bash
# dma 库
cd components/system/dma
mkdir build && cd build
cmake ..
cmake --build .

# shm 库
cd components/system/shm
mkdir build && cd build
cmake ..
cmake --build .

# sys 库
cd components/system/sys
mkdir build && cd build
cmake ..
cmake --build .

# dma 内核驱动（需内核头文件）
cd components/system/dma/ko
mkdir build && cd build
cmake ..
cmake --build .
```

通过 SDK mm 编译（若已集成）：

```bash
source build/envsetup.sh
lunch <target>
cd components/system/dma/ko
mm clean && mm -v
```

### 运行示例

- **dma**：`./build/output/test_dma --help`（需先加载 me_dma.ko）
- **shm**：`./build/output/shm_transport_pub_test` / `shm_transport_sub_test`
- **sys**：`./build/test_sys`（部分测试需 root 权限）

## 详细使用

- [dma 库与内核驱动](dma/README.md)、[me_dma 驱动](dma/ko/README.md)
- [shm 共享内存](shm/README.md)
- [sys 系统控制](sys/README.md)

## 常见问题

1. **me_dma.ko 加载失败**：确认内核已启用 DMA-HEAP，且目标板支持对应 DMA 引擎。
2. **shm 分配失败**：检查 `/dev/dma_heap/*` 是否存在，权限是否足够。
3. **sys 看门狗/唤醒测试失败**：需 root 权限，且设备上存在 `/dev/watchdog`、GPIO sysfs。

## 版本与发布

随 SDK 版本发布，具体 tag 见仓库 release 记录。

## License

本组件源码文件头声明为 Apache-2.0，最终以本目录 `LICENSE` 文件为准。
