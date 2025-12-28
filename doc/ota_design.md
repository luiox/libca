# em_ota 组件深度设计分析

## 1. 分区布局设计 (Partition Layout)
为了实现 AAB 分区和 Bootloader 备份，建议的 Flash 布局如下：

| 分区名称 | 存储介质 | 说明 |
| :--- | :--- | :--- |
| **Bootloader_A** | 内部 Flash | 当前运行的启动程序 |
| **Bootloader_B** | 内部 Flash | Bootloader 备份区 (用于 Bootloader 自身升级) |
| **App_A** | 内部 Flash | 应用程序运行区 A |
| **App_B** | 内部 Flash | 应用程序运行区 B |
| **OTA_Config** | 内部 Flash | 存储升级标志位、版本号、校验和等元数据 |
| **Download_Cache**| 外部 W25Q64 | 接收固件时的临时缓冲区 (可选，若内部 Flash 足够可直接写入 B 区) |
| **Log_Storage** | 外部 W25Q64 | FatFs 文件系统，存储 EasyLogger 日志 |

## 2. 核心组件：分区管理器 (Partition Manager)
`em_ota` 不应直接调用 `HAL_Flash_Write`。需要一个抽象层：

```c
typedef struct {
    const char* name;
    uint32_t    start_addr;
    uint32_t    size;
    uint8_t     device_id; // 0: Internal, 1: W25Q64
} partition_t;

// 统一接口
int partition_read(int part_id, uint32_t offset, void* buf, uint32_t len);
int partition_write(int part_id, uint32_t offset, const void* buf, uint32_t len);
int partition_erase(int part_id, uint32_t offset, uint32_t len);
```

## 3. 升级流程逻辑

### 3.1 App 升级流程 (由 Bootloader 或 App 触发)
1.  **下载**：通过 `em_protocol` (Ymodem) 接收数据，流式写入 `App_B`。
2.  **校验**：计算 `App_B` 的 CRC32，与上位机传来的校验值比对。
3.  **标记**：在 `OTA_Config` 分区写入“App_B 待更新”标志。
4.  **重启**：触发软件复位。
5.  **跳转 (Bootloader 逻辑)**：
    *   检查 `OTA_Config`。
    *   若 `App_B` 有效，则将 `App_B` 拷贝到 `App_A` (或者直接跳转到 `App_B`，取决于是否支持中断向量表重定向)。
    *   更新成功后清除标志。

### 3.2 Bootloader 升级流程 (高风险操作)
1.  **备份**：App 将当前 `Bootloader_A` 拷贝到 `Bootloader_B`。
2.  **下载**：App 接收新 Bootloader 固件并写入 `Bootloader_A` 的临时区（或直接覆盖，但建议先写到备份区）。
3.  **校验**：确保新 Bootloader 完整。
4.  **更新**：App 执行擦除并写入 `Bootloader_A`。
5.  **恢复机制**：如果 `Bootloader_A` 损坏，硬件需要某种方式（如按键触发或看门狗）强制运行备份区的 `Bootloader_B`（这通常需要硬件支持或极小的引导代码）。

## 4. 状态机设计 (OTA State Machine)
`em_ota` 模块应维护一个全局状态：
- `OTA_IDLE`: 空闲
- `OTA_READY`: 收到升级指令，准备就绪
- `OTA_RECEIVING`: 正在接收数据 (对接 `em_protocol`)
- `OTA_DECOMPRESSING`: 解压 (如果支持压缩升级)
- `OTA_VERIFYING`: 校验中
- `OTA_APPLYING`: 应用更新中
- `OTA_FAULT`: 错误状态

## 5. 关键技术点分析
*   **原子性 (Atomicity)**：升级标志位的写入必须是原子的。建议使用两个互补的标志位，防止写入中途掉电。
*   **版本回滚**：在 `OTA_Config` 中记录 `Previous_Version`。如果新版本启动后 30 秒内未调用 `ota_confirm()`，则自动回滚。
*   **中断向量表**：跳转到 App 前，必须关闭所有中断，重置堆栈指针，并设置 `SCB->VTOR`。
