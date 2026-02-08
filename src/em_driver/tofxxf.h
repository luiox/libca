/**
 * @file tofxxf.h
 * @author canrad (1517807724@qq.com)
 * @brief TOFxxF系列传感器 Modbus RTU 驱动
 * 
 * 支持 TOF050F/TOF200F/TOF400F，使用标准 Modbus RTU 协议通信。
 * 串口参数：115200bps, 8N1
 * 
 * @version 0.1
 * @date 2026-02-08
 * 
 * @copyright Copyright (c) 2026
 */
#ifndef LIBCA_EM_DRIVER_TOFXF_H
#define LIBCA_EM_DRIVER_TOFXF_H

#include "../em_base/datatype.h"

// 默认波特率
#define TOFXF_BAUDRATE_DEFAULT  115200

// 功能码
#define TOFXF_FUNC_READ_HOLDING 0x03    /* 读取保持寄存器 */
#define TOFXF_FUNC_WRITE_SINGLE 0x06    /* 写单个保持寄存器 */

// 寄存器地址
#define TOFXF_REG_RESTORE       0x0001  /* 恢复默认参数 */
#define TOFXF_REG_DEVICE_ADDR   0x0002  /* 设备地址 */
#define TOFXF_REG_BAUDRATE      0x0003  /* 波特率 */
#define TOFXF_REG_MODE          0x0004  /* 量程模式 */
#define TOFXF_REG_AUTO_OUTPUT   0x0005  /* 连续输出控制 */
#define TOFXF_REG_CALIB_LOAD    0x0006  /* 加载校准 */
#define TOFXF_REG_OFFSET        0x0007  /* 偏移修正值 */
#define TOFXF_REG_XTALK         0x0008  /* xtalk修正值 */
#define TOFXF_REG_IIC_DISABLE   0x0009  /* 禁止IIC使能 */
#define TOFXF_REG_DISTANCE      0x0010  /* 测量结果 (mm) */

// 量程模式
#define TOFXF_MODE_DEFAULT      0x0000  /* 默认模式 */
#define TOFXF_MODE_HIGH_PREC    0x0001  /* 高精度模式 */
#define TOFXF_MODE_LONG_RANGE   0x0002  /* 长距模式 */
#define TOFXF_MODE_HIGH_SPEED   0x0003  /* 高速模式 */

// 波特率设置值
#define TOFXF_BAUD_38400        0x0001
#define TOFXF_BAUD_9600         0x0002
#define TOFXF_BAUD_115200       0x0003

// 错误码 (负数)
#define TOFXF_OK                0
#define TOFXF_ERR_PORT          -1      /* 端口未注册 */
#define TOFXF_ERR_SEND          -2      /* 发送失败 */
#define TOFXF_ERR_RECV          -3      /* 接收失败 */
#define TOFXF_ERR_TIMEOUT       -4      /* 超时 */
#define TOFXF_ERR_CRC           -5      /* CRC校验错误 */
#define TOFXF_ERR_RESPONSE      -6      /* 响应格式错误 */
#define TOFXF_ERR_SLAVE_ADDR    -7      /* 从机地址错误 */
#define TOFXF_ERR_FUNC_CODE     -8      /* 功能码错误 */

/* ========== Port层 ========== */

typedef struct tofxxf_port {
    /**
     * @brief 发送数据到串口
     * @param huart 串口句柄
     * @param data 数据缓冲区
     * @param len 数据长度
     * @return 成功返回发送字节数，失败返回负值
     */
    i32 (*uart_send)(void* huart, const u8* data, usize len);
    
    /**
     * @brief 从串口接收数据（阻塞模式）
     * @param huart 串口句柄
     * @param buf 接收缓冲区
     * @param len 期望接收长度
     * @param timeout_ms 超时时间（毫秒）
     * @return 成功返回接收字节数，失败返回负值
     */
    i32 (*uart_recv)(void* huart, u8* buf, usize len, u32 timeout_ms);
    
    /**
     * @brief 延时函数
     * @param ms 延时毫秒数
     */
    void (*delay_ms)(u32 ms);
} tofxxf_port_t;

void tofxxf_bind_port(const tofxxf_port_t* port);
bool tofxxf_port_is_registered(void);

/* ========== 驱动层 ========== */

typedef struct tofxxf {
    void* huart;        /* 串口句柄 */
    u8 slave_addr;      /* 从机地址 */
} tofxxf_t;

void tofxxf_init(tofxxf_t* self, void* huart, u8 slave_addr);

/**
 * @brief 读取保持寄存器 (功能码 0x03)
 * @param self 设备实例
 * @param reg_addr 寄存器地址
 * @param count 读取寄存器数量
 * @param out_buf 输出缓冲区（大端格式）
 * @param out_buf_size 输出缓冲区大小
 * @return 成功返回0，失败返回错误码
 */
i32 tofxxf_read_reg(tofxxf_t* self, u16 reg_addr, 
                     u16 count, u8* out_buf, usize out_buf_size);

/**
 * @brief 写入单个保持寄存器 (功能码 0x06)
 * @param self 设备实例
 * @param reg_addr 寄存器地址
 * @param value 写入值
 * @return 成功返回0，失败返回错误码
 */
i32 tofxxf_write_reg(tofxxf_t* self, u16 reg_addr, u16 value);

/**
 * @brief 读取距离值（mm）
 * @param self 设备实例
 * @param distance 输出距离值
 * @return 成功返回0，失败返回错误码
 */
i32 tofxxf_get_distance(tofxxf_t* self, u16* distance);

/**
 * @brief 设置量程模式
 * @param self 设备实例
 * @param mode 模式（TOFXF_MODE_*）
 * @return 成功返回0，失败返回错误码
 */
i32 tofxxf_set_mode(tofxxf_t* self, u16 mode);

/**
 * @brief 设置设备地址
 * @param self 设备实例
 * @param addr 新地址
 * @return 成功返回0，失败返回错误码
 */
i32 tofxxf_set_address(tofxxf_t* self, u8 addr);

/**
 * @brief 设置波特率
 * @param self 设备实例
 * @param baud 波特率代码（TOFXF_BAUD_*）
 * @return 成功返回0，失败返回错误码
 * @note 设置后需要重启设备生效
 */
i32 tofxxf_set_baudrate(tofxxf_t* self, u16 baud);

/**
 * @brief 设置自动输出间隔
 * @param self 设备实例
 * @param interval_ms 间隔时间（毫秒）
 * @return 成功返回0，失败返回错误码
 */
i32 tofxxf_set_auto_output(tofxxf_t* self, u16 interval_ms);

/**
 * @brief 恢复出厂设置
 * @param self 设备实例
 * @return 成功返回0，失败返回错误码
 */
i32 tofxxf_restore_default(tofxxf_t* self);

#endif // !LIBCA_EM_DRIVER_TOFXF_H
