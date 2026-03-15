#include "atk_ms53l1m.h"
#include "em_base/debug.h"
#include <string.h>

#define ATK_MS53L1M_MASTER_FRAME_HEAD 0x51 /* 主机请求帧头 */
#define ATK_MS53L1M_SLAVE_FRAME_HEAD 0x55  /* 从机应答帧头 */
#define ATK_MS53L1M_SENSOR_TYPE 0x0B       /* ATK-MS53L1M传感器类型 */

#define ATK_MS53L1M_FRAME_LEN_MAX 270 /* 接收帧最大长度 */
#define ATK_MS53L1M_FRAME_LEN_MIN 8   /* 接收帧最小长度 */

#define ATK_MS53L1M_OPT_READ 0x00  /* 读操作 */
#define ATK_MS53L1M_OPT_WRITE 0x01 /* 写操作 */

/* ATK-MS53L1M模块功能码 */
enum
{
    ATK_MS53L1M_FUNCODE_SYS          = 0x00, /* 系统设置 */
    ATK_MS53L1M_FUNCODE_BACKRATE     = 0x01, /* 回传速率设置 */
    ATK_MS53L1M_FUNCODE_BAUDRATE     = 0x02, /* 波特率设置 */
    ATK_MS53L1M_FUNCODE_IDSET        = 0x03, /* 设备地址设置 */
    ATK_MS53L1M_FUNCODE_MEAUDATA     = 0x05, /* 测量数据获取 */
    ATK_MS53L1M_FUNCODE_OUTPUTSTATUS = 0x07, /* 测量状态 */
    ATK_MS53L1M_FUNCODE_MEAUMODE     = 0x08, /* 测量模式设置 */
    ATK_MS53L1M_FUNCODE_CALIMODE     = 0x09, /* 校准模式 */
    ATK_MS53L1M_FUNCODE_WORKMODE     = 0x0A, /* 工作模式 */
    ATK_MS53L1M_FUNCODE_TIMEBUDGET   = 0x0B, /* 定时预设 */
    ATK_MS53L1M_FUNCODE_TIMRPERIOD   = 0x0D, /* 测量间隔 */
    ATK_MS53L1M_FUNCODE_ERRORFRAM    = 0x0F, /* 错误帧信息 */
    ATK_MS53L1M_FUNCODE_VERSION      = 0x10, /* 版本信息 */
};

static atk_ms53l1m_port_t g_port            = {0};
static bool               g_port_registered = false;

/* 绑定port */
void atk_ms53l1m_bind_port(const atk_ms53l1m_port_t* port)
{
    if (port != NULL) {
        g_port            = *port;
        g_port_registered = true;
    }
}

/* 检查port是否已注册 */
bool atk_ms53l1m_port_is_registered(void)
{
    return g_port_registered;
}

/* 计算CRC校验和 */
static inline u16 atk_ms53l1m_crc_check_sum(u8* buf, u16 len)
{
    u16 check_sum = 0;
    u16 i;

    for (i = 0; i < len; i++) {
        check_sum += buf[i];
    }

    return check_sum;
}

/* 解析接收到的数据包 */
static i32 atk_ms53l1m_unpack_recv_data(u16* dat)
{
    u8* frame_buf  = NULL;
    u16 timeout    = 0;
    u16 recv_len   = 0;
    u16 frame_loop = 0;
    u16 frame_head_index;
    u16 frame_len;
    u8  opt_type;
    u16 dat_len;
    u16 frame_check_sum;
    u16 check_sum;

    while (frame_buf == NULL) {
        /* 等待ATK-MS53L1M UART接收到一帧数据 */
        frame_buf = g_port.uart_rx_get_frame();
        g_port.delay_ms(1);
        timeout++;
        if (timeout == 1000) {
            /* 接收超时错误 */
            return ATK_MS53L1M_ERR_TIMEOUT;
        }
    }

    /* 获取接收数据的长度 */
    recv_len = g_port.uart_rx_get_frame_len();
    if ((recv_len < ATK_MS53L1M_FRAME_LEN_MIN) || (recv_len > ATK_MS53L1M_FRAME_LEN_MAX)) {
        /* 接收帧长度异常错误 */
        return ATK_MS53L1M_ERR_FRAME;
    }

    /* 查找帧头 */
    do {
        if ((frame_buf[frame_loop] == ATK_MS53L1M_SLAVE_FRAME_HEAD) &&
            (frame_buf[frame_loop + 1] == ATK_MS53L1M_SENSOR_TYPE)) {
            break;
        }

        if (frame_loop != (recv_len - 2)) {
            frame_loop++;
        }
        else {
            /* 帧异常 */
            return ATK_MS53L1M_ERR_FRAME;
        }
    } while (1);

    frame_head_index = frame_loop;                  /* 记录帧头位置 */
    frame_len        = recv_len - frame_head_index; /* 计算帧长度 */

    if ((frame_len < ATK_MS53L1M_FRAME_LEN_MIN) || (frame_len > ATK_MS53L1M_FRAME_LEN_MAX)) {
        /* 接收帧长度异常错误 */
        return ATK_MS53L1M_ERR_FRAME;
    }

    opt_type = frame_buf[frame_head_index + 4]; /* 获取操作类型 */

    if (opt_type == 0x00) {
        dat_len = frame_buf[frame_head_index + 7]; /* 获取数据长度 */
        if ((dat_len + 10) > frame_len) {
            /* 帧异常 */
            return ATK_MS53L1M_ERR_FRAME;
        }

        frame_check_sum = atk_ms53l1m_crc_check_sum(&frame_buf[frame_head_index],
                                                    dat_len + 8); /* 计算CRC校验和 */
        check_sum       = ((u16)frame_buf[frame_head_index + dat_len + 8] << 8) +
                    frame_buf[frame_head_index + dat_len + 9]; /* 获取帧的CRC校验和 */
        if (frame_check_sum == check_sum) {
            if (frame_buf[frame_head_index + 7] == 1) {
                *dat = frame_buf[frame_head_index + 8];
            }
            else if (frame_buf[frame_head_index + 7] == 2) {
                *dat =
                    ((u16)frame_buf[frame_head_index + 8] << 8) + frame_buf[frame_head_index + 9];
            }
            else {
                /* 帧错误 */
                return ATK_MS53L1M_ERR_FRAME;
            }

            return ATK_MS53L1M_OK;
        }
        else {
            /* CRC错误 */
            return ATK_MS53L1M_ERR_CRC;
        }
    }
    else if (opt_type == 0x01) {
        frame_check_sum =
            atk_ms53l1m_crc_check_sum(&frame_buf[frame_head_index], 6); /* 计算CRC校验和 */
        check_sum = ((u16)frame_buf[frame_head_index + 6] << 8) + frame_buf[frame_head_index + 7];
        if (frame_check_sum == check_sum) {
            return ATK_MS53L1M_OK;
        }
        else {
            /* CRC错误 */
            return ATK_MS53L1M_ERR_CRC;
        }
    }
    else if (opt_type == 0xFF) {
        if ((frame_buf[frame_head_index + 2] == 0xFF) &&
            (frame_buf[frame_head_index + 3] == 0xFF)) {
            frame_check_sum =
                atk_ms53l1m_crc_check_sum(&frame_buf[frame_head_index], 6); /* 计算CRC校验和 */
            check_sum =
                ((u16)frame_buf[frame_head_index + 6] << 8) + frame_buf[frame_head_index + 7];
            if (frame_check_sum == check_sum) {
                /* 异常操作 */
                return ATK_MS53L1M_ERR_OPT;
            }
            else {
                /* CRC错误 */
                return ATK_MS53L1M_ERR_CRC;
            }
        }
        else {
            /* 帧异常 */
            return ATK_MS53L1M_ERR_FRAME;
        }
    }
    else {
        /* 帧异常 */
        return ATK_MS53L1M_ERR_FRAME;
    }
}

/* 根据模块功能码读取数据 */
static i32 atk_ms53l1m_read_data(atk_ms53l1m_t* self, u16 addr, u8 fun_code, u8 len, u16* dat)
{
    param_check(self);
    i32 ret;
    u16 check_sum;
    u8  buf[9];

    buf[0] = ATK_MS53L1M_MASTER_FRAME_HEAD; /* 标志头 */
    buf[1] = ATK_MS53L1M_SENSOR_TYPE;       /* 传感器类型 */
    buf[2] = (u8)(addr >> 8);               /* 传感器地址，高8位 */
    buf[3] = (u8)(addr & 0xFF);             /* 传感器地址，低8位 */
    buf[4] = ATK_MS53L1M_OPT_READ;          /* 读操作 */
    buf[5] = fun_code;                      /* 功能码 */
    buf[6] = len;                           /* 数据长度 */

    check_sum = atk_ms53l1m_crc_check_sum(buf, 7); /* 计算CRC校验和 */

    buf[7] = (u8)(check_sum >> 8);   /* CRC校验码，高8位 */
    buf[8] = (u8)(check_sum & 0xFF); /* CRC校验码，低8位 */

    g_port.uart_rx_restart();                /* 准备重新开始接收新的一帧数据 */
    g_port.uart_send(buf, 9);                /* 发送数据 */
    ret = atk_ms53l1m_unpack_recv_data(dat); /* 解析应答数据 */

    return ret;
}

/* 根据模块功能码写入1字节数据 */
static i32 atk_ms53l1m_write_data(atk_ms53l1m_t* self, u16 addr, u8 fun_code, u8 dat)
{
    param_check(self);
    i32 ret;
    u8  buf[10];
    u16 check_sum;

    buf[0] = ATK_MS53L1M_MASTER_FRAME_HEAD; /* 标志头 */
    buf[1] = ATK_MS53L1M_SENSOR_TYPE;       /* 传感器类型 */
    buf[2] = (u8)(addr >> 8);               /* 传感器地址，高8位 */
    buf[3] = (u8)(addr & 0xFF);             /* 传感器地址，低8位 */
    buf[4] = ATK_MS53L1M_OPT_WRITE;         /* 写操作 */
    buf[5] = fun_code;                      /* 功能码 */
    buf[6] = 0x01;                          /* 数据长度 */
    buf[7] = dat;                           /* 数据 */

    check_sum = atk_ms53l1m_crc_check_sum(buf, 8); /* 计算CRC校验和 */

    buf[8] = (u8)(check_sum >> 8);   /* CRC校验码，高8位 */
    buf[9] = (u8)(check_sum & 0xFF); /* CRC校验码，低8位 */

    g_port.uart_rx_restart();                 /* 准备重新开始接收新的一帧数据 */
    g_port.uart_send(buf, 10);                /* 发送数据 */
    ret = atk_ms53l1m_unpack_recv_data(NULL); /* 解析应答数据 */

    return ret;
}

/* ATK-MS53L1M初始化 */
i32 atk_ms53l1m_init(atk_ms53l1m_t* self, u32 baudrate, atk_ms53l1m_mode_t work_mode)
{
    param_check(self);
    u8 i;

    if (!g_port_registered) {
        debug_print("[atkms53l1m] error: port not registered\n");
        return ATK_MS53L1M_ERR;
    }

    self->baudrate  = baudrate;
    self->work_mode = work_mode;

    /* ATK-MS53L1M UART初始化 */
    g_port.uart_init(baudrate);

    /* 获取设备地址 */
    i = 0;
    while (atk_ms53l1m_read_data(self, 0xFFFF, ATK_MS53L1M_FUNCODE_IDSET, 2, &self->device_id) !=
           ATK_MS53L1M_OK) {
        g_port.delay_ms(100);
        if (++i == 5) {
            debug_print("[atkms53l1m] error: get device id timeout\n");
            return ATK_MS53L1M_ERR;
        }
    }

    /* 设置ATK-MS53L1M模块的工作模式 */
    i = 0;
    while (atk_ms53l1m_write_data(self, self->device_id, ATK_MS53L1M_FUNCODE_WORKMODE, work_mode) !=
           ATK_MS53L1M_OK) {
        g_port.delay_ms(100);
        if (++i == 5) {
            debug_print("[atkms53l1m] error: set work mode timeout\n");
            return ATK_MS53L1M_ERR;
        }
    }

    return ATK_MS53L1M_OK;
}

/* ATK-MS53L1M Normal工作模式获取测量值 */
i32 atk_ms53l1m_normal_get_data(atk_ms53l1m_t* self, u16* dat)
{
    param_check(self);
    if (dat == NULL) {
        return ATK_MS53L1M_ERR;
    }

    u8*   buf = NULL;
    u8    i   = 0;
    char* p;
    u16   dat_tmp = 0;

    g_port.uart_rx_restart();
    while (buf == NULL) {
        buf = g_port.uart_rx_get_frame();
        if (++i == 10) {
            debug_print("[atkms53l1m] error: receive data timeout in normal mode\n");
            return ATK_MS53L1M_ERR;
        }
        g_port.delay_ms(100);
    }

    p = strstr((char*)buf, "d:");
    if (p == NULL) {
        debug_print("[atkms53l1m] error: invalid data format in normal mode\n");
        return ATK_MS53L1M_ERR;
    }

    while (*p != 'm') {
        if (*p >= '0' && *p <= '9') {
            dat_tmp = dat_tmp * 10 + (*p - '0');
        }
        p++;
    }

    *dat = dat_tmp;

    return ATK_MS53L1M_OK;
}

/* ATK-MS53L1M Modbus工作模式获取测量值 */
i32 atk_ms53l1m_modbus_get_data(atk_ms53l1m_t* self, u16* dat)
{
    param_check(self);
    if (dat == NULL) {
        return ATK_MS53L1M_ERR;
    }

    i32 ret;
    u16 dat_tmp;

    ret = atk_ms53l1m_read_data(self, self->device_id, ATK_MS53L1M_FUNCODE_MEAUDATA, 2, &dat_tmp);
    if (ret != ATK_MS53L1M_OK) {
        *dat = 0;
        debug_print("[atkms53l1m] error: get data failed in modbus mode, ret:%d\n", ret);
        return ATK_MS53L1M_ERR;
    }
    else {
        *dat = dat_tmp;
        return ATK_MS53L1M_OK;
    }
}
