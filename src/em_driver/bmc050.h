/**
 * @file bmc050.h
 * @author canrad (1517807724@qq.com)
 * @brief BMC050 三轴加速度 + 磁力计 驱动
 * @version 0.1
 * @date 2026-01-22
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef LIBCA_EM_DRIVER_BMC050_H
#define LIBCA_EM_DRIVER_BMC050_H

#include "em_base/datatype.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bmc050_port {
    i32 (*i2c_write)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, const u8* data, u16 data_size, u32 timeout);
    i32 (*i2c_read)(void* hi2c, u16 dev_addr, u16 mem_addr, u16 mem_addr_size, u8* data, u16 data_size, u32 timeout);
    void (*delay_us)(u32 us);
} bmc050_port_t;

void bmc050_bind_port(const bmc050_port_t* port);
bool bmc050_port_is_registered(void);

// 量程 / 带宽 / 中断 等枚举
typedef enum bmc050_acc_fs_enum {
    BMC050_ACC_FS_2G  = 0x03,
    BMC050_ACC_FS_4G  = 0x05,
    BMC050_ACC_FS_8G  = 0x08,
    BMC050_ACC_FS_16G = 0x0c
} bmc050_acc_fs; 

typedef enum bmc050_acc_sleep_enum {
    BMC050_ACC_SLEEP_0R5  = 0x00,
    BMC050_ACC_SLEEP_1    = 0x0c,
    BMC050_ACC_SLEEP_2    = 0x0e,
    BMC050_ACC_SLEEP_4    = 0x10,
    BMC050_ACC_SLEEP_6    = 0x12,
    BMC050_ACC_SLEEP_10   = 0x14,
    BMC050_ACC_SLEEP_25   = 0x16,
    BMC050_ACC_SLEEP_50   = 0x18,
    BMC050_ACC_SLEEP_100  = 0x1a,
    BMC050_ACC_SLEEP_500  = 0x1c,
    BMC050_ACC_SLEEP_1000 = 0x1e
} bmc050_acc_sleep; 

typedef enum bmc050_acc_bw_enum {
    BMC050_ACC_BW8    = 0x08,
    BMC050_ACC_BW16   = 0x09,
    BMC050_ACC_BW31   = 0x0a,
    BMC050_ACC_BW63   = 0x0b,
    BMC050_ACC_BW125  = 0x0c,
    BMC050_ACC_BW250  = 0x0d,
    BMC050_ACC_BW500  = 0x0e,
    BMC050_ACC_BW1000 = 0x0f,
    BMC050_ACC_BW2000 = 0x80
} bmc050_acc_bw; 

typedef enum bmc050_acc_ie_enum {
    BMC050_ACC_IE_DISABLE = 0x0000,
    BMC050_ACC_IE_FLAT    = 0x8000,
    BMC050_ACC_IE_ORIENT  = 0x4000,
    BMC050_ACC_IE_STAP    = 0x2000,
    BMC050_ACC_IE_DTAP    = 0x1000,
    BMC050_ACC_IE_SLOPEZ  = 0x0400,
    BMC050_ACC_IE_SLOPEY  = 0x0200,
    BMC050_ACC_IE_SLOPEX  = 0x0100,
    BMC050_ACC_IE_DATA    = 0x0010,
    BMC050_ACC_IE_LG      = 0x0008,
    BMC050_ACC_IE_HGZ     = 0x0004,
    BMC050_ACC_IE_HGY     = 0x0002,
    BMC050_ACC_IE_HGX     = 0x0001
} bmc050_acc_ie; 

typedef enum bmc050_acc_irq_enum {
    BMC050_ACC_IRQ_NONE   = 0x0000,
    BMC050_ACC_IRQ_DATA   = 0x8000,
    BMC050_ACC_IRQ_FLAT   = 0x0080,
    BMC050_ACC_IRQ_ORIENT = 0x0040,
    BMC050_ACC_IRQ_STAP   = 0x0020,
    BMC050_ACC_IRQ_DTAP   = 0x0010,
    BMC050_ACC_IRQ_SLOPE  = 0x0004,
    BMC050_ACC_IRQ_HG     = 0x0002,
    BMC050_ACC_IRQ_LG     = 0x0001
} bmc050_acc_irq; 

typedef enum bmc050_acc_ts_enum {
    BMC050_ACC_TS_TAPNEG   = 0x80,
    BMC050_ACC_TS_TAPZ     = 0x40,
    BMC050_ACC_TS_TAPY     = 0x20,
    BMC050_ACC_TS_TAPX     = 0x10,
    BMC050_ACC_TS_SLOPENEG = 0x08,
    BMC050_ACC_TS_SLOPEZ   = 0x04,
    BMC050_ACC_TS_SLOPEY   = 0x02,
    BMC050_ACC_TS_SLOPEX   = 0x01
} bmc050_acc_ts; 

typedef enum bmc050_acc_fo_enum {
    BMC050_ACC_FO_FLAT      = 0x80,
    BMC050_ACC_FO_DOWNWARD  = 0x40,
    BMC050_ACC_FO_PUPRIGHT  = 0x00,
    BMC050_ACC_FO_PUPDOWN   = 0x10,
    BMC050_AFF_FO_LANDLEFT  = 0x20,
    BMC050_AFF_FO_LANDRIGHT = 0x30,
    BMC050_ACC_FO_SLOPENEG  = 0x08,
    BMC050_ACC_FO_HGZ       = 0x04,
    BMC050_ACC_FO_HGY       = 0x02,
    BMC050_ACC_FO_HGX       = 0x01
} bmc050_acc_fo; 

typedef enum bmc050_acc_im_enum {
    BMC050_ACC_IM_RESET     = 0x80,
    BMC050_ACC_IM_NOLATCH   = 0x00,
    BMC050_ACC_IM_500us     = 0x09,
    BMC050_ACC_IM_1ms       = 0x0b,
    BMC050_ACC_IM_12ms      = 0x0c,
    BMC050_ACC_IM_25ms      = 0x0d,
    BMC050_ACC_IM_50ms      = 0x0e,
    BMC050_ACC_IM_250ms     = 0x01,
    BMC050_ACC_IM_500ms     = 0x02,
    BMC050_ACC_IM_1s        = 0x03,
    BMC050_ACC_IM_2s        = 0x04,
    BMC050_ACC_IM_4s        = 0x05,
    BMC050_ACC_IM_8s        = 0x06,
    BMC050_ACC_IM_LATCH     = 0x0f
} bmc050_acc_im; 

typedef enum bmc050_acc_if_enum {
    BMC050_ACC_IF_WDT_OFF   = 0x00,
    BMC050_ACC_IF_WDT_1ms   = 0x04,
    BMC050_ACC_IF_WDT_50ms  = 0x06
} bmc050_acc_if; 

typedef enum bmc050_acc_intconfig_enum {
    BMC050_ACC_INT1_OD   = 0x02,
    BMC050_ACC_INT1_PP   = 0x00,
    BMC050_ACC_INT1_LOW  = 0x00,
    BMC050_ACC_INT1_HIGH = 0x01,
    BMC050_ACC_INT2_OD   = 0x08,
    BMC050_ACC_INT2_PP   = 0x00,
    BMC050_ACC_INT2_LOW  = 0x00,
    BMC050_ACC_INT2_HIGH = 0x04
} bmc050_acc_intconfig; 

typedef enum bmc050_acc_intmap_enum {
    BMC050_ACC_IM1_FLAT   = 0x800000,
    BMC050_ACC_IM1_ORIENT = 0x400000,
    BMC050_ACC_IM1_STAP   = 0x200000,
    BMC050_ACC_IM1_DTAP   = 0x100000,
    BMC050_ACC_IM1_SLOPE  = 0x040000,
    BMC050_ACC_IM1_HIGHG  = 0x020000,
    BMC050_ACC_IM1_LOWG   = 0x010000,
    BMC050_ACC_IM1_DATA   = 0x000100,
    BMC050_ACC_IM2_FLAT   = 0x000080,
    BMC050_ACC_IM2_ORIENT = 0x000040,
    BMC050_ACC_IM2_STAP   = 0x000020,
    BMC050_ACC_IM2_DTAP   = 0x000010,
    BMC050_ACC_IM2_SLOPE  = 0x000004,
    BMC050_ACC_IM2_HIGHG  = 0x000002,
    BMC050_ACC_IM2_LOWG   = 0x000001,
    BMC050_ACC_IM2_DATA   = 0x008000
} bmc050_acc_intmap; 

// --- object ---
typedef struct bmc050 {
    void* hi2c; // i2c 句柄
    u16   dev_addr; // 设备 8-bit 地址（例如 0x36）
} bmc050_t;

// 错误码
#define BMC050_OK 0
#define BMC050_ERR_PORT_NOT_REGISTERED (-1)
#define BMC050_ERR_I2C_FAIL (-2)
#define BMC050_ERR_INVALID_PARAM (-3)

/**
 * @brief 初始化 bmc050 对象
 * @param self 对象
 * @param hi2c 底层 i2c 句柄
 * @param dev_addr 设备 8-bit 地址（例如 0x36）
 */
void bmc050_init(bmc050_t* self, void* hi2c, u16 dev_addr);

/**
 * @brief 读取加速度芯片 ID
 * @return BMC050_OK 或 错误码
 */
i32 bmc050_get_device_id(bmc050_t* self, u8* id);

/**
 * @brief 读取温度，单位：0.1°C（例如 245 表示 24.5°C）
 */
i32 bmc050_read_temperature(bmc050_t* self, int16_t* temp10);

/**
 * @brief 设置加速度量程
 */
i32 bmc050_set_range(bmc050_t* self, bmc050_acc_fs range);

/**
 * @brief 设置带宽
 */
i32 bmc050_set_bandwidth(bmc050_t* self, bmc050_acc_bw bw);

i32 bmc050_soft_reset(bmc050_t* self);
i32 bmc050_power_normal(bmc050_t* self);
i32 bmc050_suspend(bmc050_t* self);
i32 bmc050_low_power(bmc050_t* self, bmc050_acc_sleep sleep_duration);

/**
 * @brief 读取单轴/三轴数据
 */
i32 bmc050_get_x(bmc050_t* self, int16_t* x);
i32 bmc050_get_y(bmc050_t* self, int16_t* y);
i32 bmc050_get_z(bmc050_t* self, int16_t* z);
i32 bmc050_get_xyz(bmc050_t* self, int16_t* x, int16_t* y, int16_t* z);

/**
 * @brief 中断配置接口
 */
i32 bmc050_set_irq(bmc050_t* self, bmc050_acc_ie irqs);
i32 bmc050_get_irq_status(bmc050_t* self, bmc050_acc_irq* status);
i32 bmc050_set_irq_mode(bmc050_t* self, bmc050_acc_im mode);
i32 bmc050_config_slope_irq(bmc050_t* self, u8 nSamples, u8 threshold);
i32 bmc050_get_ts_irq(bmc050_t* self, u8* status);
i32 bmc050_interface_config(bmc050_t* self, bmc050_acc_if mode);
i32 bmc050_int_pin_config(bmc050_t* self, bmc050_acc_intconfig mode);
i32 bmc050_int_pin_map(bmc050_t* self, bmc050_acc_intmap map);

/**
 * @brief 磁力计 ID 读取
 */
i32 bmc050_mag_get_device_id(bmc050_t* self, u8* id);

#ifdef __cplusplus
}
#endif

#endif // LIBCA_EM_DRIVER_BMC050_H
