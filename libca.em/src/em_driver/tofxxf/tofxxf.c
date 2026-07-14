#include "tofxxf.h"
#include <em_base/debug.h>
#include <em_util/crc.h>
#include <string.h>

////////////////////////////////////////////////////////////////////////////////

#if (LIBCA_TOFXXF_PORT_MODE == LIBCA_TOFXXF_PORT_MODE_EXTERN)
#define TOFXXF_UART_SEND(huart, data, len)            port_tofxxf_uart_send((huart), (data), (len))
#define TOFXXF_UART_RECV(huart, buf, len, timeout_ms) port_tofxxf_uart_recv((huart), (buf), (len), (timeout_ms))

#elif (LIBCA_TOFXXF_PORT_MODE == LIBCA_TOFXXF_PORT_MODE_DYNAMIC)
static const tofxxf_port_t* g_tofxxf_port = NULL;
#define TOFXXF_UART_SEND(huart, data, len)            g_tofxxf_port->uart_send((huart), (data), (len))
#define TOFXXF_UART_RECV(huart, buf, len, timeout_ms) g_tofxxf_port->uart_recv((huart), (buf), (len), (timeout_ms))

#else
#error "Invalid TOFXXF port mode"
#endif

#if (LIBCA_TOFXXF_PORT_MODE == LIBCA_TOFXXF_PORT_MODE_DYNAMIC)
void tofxxf_bind_port(const tofxxf_port_t* port)
{
    g_tofxxf_port = port;
}

bool tofxxf_port_is_registered(void)
{
    return g_tofxxf_port != NULL;
}
#endif

////////////////////////////////////////////////////////////////////////////////

void tofxxf_init(tofxxf_t* self, void* huart, u8 slave_addr)
{
    param_check(self != NULL);
    param_check(huart != NULL);
    
    self->huart = huart;
    self->slave_addr = slave_addr;
}

 /// @brief 构建 Modbus RTU 请求帧
 /// 帧格式：地址(1) + 功能码(1) + 数据(N) + CRC(2)
static usize build_request_frame(u8 slave_addr, u8 func, 
                                  const u8* data, usize data_len,
                                  u8* out_buf, usize out_buf_size)
{
    if (out_buf_size < data_len + 4) {
        return 0;
    }
    
    out_buf[0] = slave_addr;
    out_buf[1] = func;
    
    memcpy(&out_buf[2], data, data_len);
    
    // 使用 em_util/crc 计算 CRC16-MODBUS
    u16 crc = crc16_modbus_fast(out_buf, 2 + data_len);
    
    // CRC 低字节在前，高字节在后（Modbus 小端序）
    out_buf[2 + data_len] = (u8)(crc & 0xFF);
    out_buf[3 + data_len] = (u8)(crc >> 8);
    
    return 4 + data_len;
}

 /// @brief 清空串口接收缓冲区
static void uart_flush_rx(void* huart)
{
    u8 dummy;
    // 非阻塞方式读取并丢弃缓冲区中的所有数据
    while (TOFXXF_UART_RECV(huart, &dummy, 1, 0) > 0) {
        // 继续清空
    }
}

 /// @brief 验证 Modbus RTU 响应帧
static i32 validate_response(const u8* frame, usize len, 
                              u8 expected_addr, u8 expected_func)
{
    if (len < 5) {
        return TOFXF_ERR_RESPONSE;
    }
    if (frame[0] != expected_addr) {
        debug_print("[tofxxf] addr mismatch: exp=0x%02X, recv=0x%02X\n", 
                    expected_addr, frame[0]);
        return TOFXF_ERR_SLAVE_ADDR;
    }
    
    // 检查是否是异常响应（功能码最高位置1）
    if (frame[1] == (expected_func | 0x80)) {
        debug_print("[tofxxf] exception response, code=0x%02X\n", frame[2]);
        return TOFXF_ERR_EXCEPTION;
    }
    
    if (frame[1] != expected_func) {
        debug_print("[tofxxf] func mismatch: exp=0x%02X, recv=0x%02X\n", 
                    expected_func, frame[1]);
        return TOFXF_ERR_FUNC_CODE;
    }
    
    // 使用 em_util/crc 验证 CRC
    u16 calc_crc = crc16_modbus_fast(frame, len - 2);
    u16 recv_crc = (u16)frame[len - 1] << 8 | frame[len - 2];
    
    if (calc_crc != recv_crc) {
        debug_print("[tofxxf] crc mismatch: calc=0x%04X, recv=0x%04X\n", 
                    calc_crc, recv_crc);
        return TOFXF_ERR_CRC;
    }
    
    return TOFXF_OK;
}

i32 tofxxf_read_reg(tofxxf_t* self, u16 reg_addr,
                     u16 count, u8* out_buf, usize out_buf_size)
{
    param_check(self != NULL);
    param_check(out_buf != NULL);
    
    // 清空接收缓冲区，避免残留数据干扰
    uart_flush_rx(self->huart);
    
    // 构建请求数据
    u8 req_data[4];
    req_data[0] = (u8)(reg_addr >> 8);
    req_data[1] = (u8)(reg_addr & 0xFF);
    req_data[2] = (u8)(count >> 8);
    req_data[3] = (u8)(count & 0xFF);
    
    // 构建请求帧
    u8 req_frame[8];
    usize req_len = build_request_frame(self->slave_addr, TOFXF_FUNC_READ_HOLDING,
                                         req_data, 4, req_frame, sizeof(req_frame));
    if (req_len == 0) {
        return TOFXF_ERR_SEND;
    }
    
    // 发送请求
    if (TOFXXF_UART_SEND(self->huart, req_frame, req_len) != (i32)req_len) {
        debug_print("[tofxxf] send failed\n");
        return TOFXF_ERR_SEND;
    }
    
    // 计算响应帧长度
    // 响应格式：地址(1) + 功能码(1) + 字节数(1) + 数据(2*count) + CRC(2)
    usize resp_len = 5 + count * 2;
    if (resp_len > 256) {
        return TOFXF_ERR_RECV;
    }
    
    u8 resp_frame[256];
    
    // 接收响应
    i32 recv_len = TOFXXF_UART_RECV(self->huart, resp_frame, resp_len, 1000);
    if (recv_len < 0) {
        debug_print("[tofxxf] recv error\n");
        return TOFXF_ERR_RECV;
    }
    if ((usize)recv_len != resp_len) {
        debug_print("[tofxxf] recv timeout\n");
        return TOFXF_ERR_TIMEOUT;
    }
    
    // 验证响应
    i32 err = validate_response(resp_frame, recv_len, self->slave_addr, 
                                 TOFXF_FUNC_READ_HOLDING);
    if (err != TOFXF_OK) {
        debug_print("[tofxxf] response validation failed, err:%d\n", err);
        return err;
    }
    
    // 验证字节数
    u8 data_bytes = resp_frame[2];
    if (data_bytes != count * 2) {
        return TOFXF_ERR_RESPONSE;
    }
    
    // 复制数据到输出缓冲区
    if (out_buf_size < data_bytes) {
        return TOFXF_ERR_RESPONSE;
    }
    
    memcpy(out_buf, &resp_frame[3], data_bytes);
    
    return TOFXF_OK;
}

i32 tofxxf_write_reg(tofxxf_t* self, u16 reg_addr, u16 value)
{
    param_check(self != NULL);
    
    // 清空接收缓冲区，避免残留数据干扰
    uart_flush_rx(self->huart);
    
    // 构建请求数据
    u8 req_data[4];
    req_data[0] = (u8)(reg_addr >> 8);
    req_data[1] = (u8)(reg_addr & 0xFF);
    req_data[2] = (u8)(value >> 8);
    req_data[3] = (u8)(value & 0xFF);
    
    // 构建请求帧
    u8 req_frame[8];
    usize req_len = build_request_frame(self->slave_addr, TOFXF_FUNC_WRITE_SINGLE,
                                         req_data, 4, req_frame, sizeof(req_frame));
    if (req_len == 0) {
        return TOFXF_ERR_SEND;
    }
    
    // 发送请求
    if (TOFXXF_UART_SEND(self->huart, req_frame, req_len) != (i32)req_len) {
        debug_print("[tofxxf] send failed\n");
        return TOFXF_ERR_SEND;
    }
    
    // 接收响应（写操作返回相同帧作为确认）
    u8 resp_frame[8];
    i32 recv_len = TOFXXF_UART_RECV(self->huart, resp_frame, req_len, 1000);
    if (recv_len < 0) {
        debug_print("[tofxxf] recv error\n");
        return TOFXF_ERR_RECV;
    }
    if ((usize)recv_len != req_len) {
        debug_print("[tofxxf] recv timeout\n");
        return TOFXF_ERR_TIMEOUT;
    }
    
    // 验证响应
    i32 err = validate_response(resp_frame, recv_len, self->slave_addr,
                                 TOFXF_FUNC_WRITE_SINGLE);
    if (err != TOFXF_OK) {
        debug_print("[tofxxf] response validation failed, err:%d\n", err);
        return err;
    }
    
    // 验证写入的数据是否一致
    if (memcmp(req_frame, resp_frame, req_len - 2) != 0) {
        return TOFXF_ERR_RESPONSE;
    }
    
    return TOFXF_OK;
}

i32 tofxxf_get_distance(tofxxf_t* self, u16* distance)
{
    param_check(distance != NULL);
    
    u8 data[2];
    i32 err = tofxxf_read_reg(self, TOFXF_REG_DISTANCE, 1, data, sizeof(data));
    if (err != TOFXF_OK) {
        return err;
    }
    
    *distance = (u16)data[0] << 8 | data[1];
    return TOFXF_OK;
}

i32 tofxxf_set_mode(tofxxf_t* self, u16 mode)
{
    return tofxxf_write_reg(self, TOFXF_REG_MODE, mode);
}

i32 tofxxf_set_address(tofxxf_t* self, u8 addr)
{
    i32 err = tofxxf_write_reg(self, TOFXF_REG_DEVICE_ADDR, addr);
    if (err == TOFXF_OK) {
        self->slave_addr = addr;
    }
    return err;
}

i32 tofxxf_set_baudrate(tofxxf_t* self, u16 baud)
{
    return tofxxf_write_reg(self, TOFXF_REG_BAUDRATE, baud);
}

i32 tofxxf_set_auto_output(tofxxf_t* self, u16 interval_ms)
{
    return tofxxf_write_reg(self, TOFXF_REG_AUTO_OUTPUT, interval_ms);
}

i32 tofxxf_restore_default(tofxxf_t* self)
{
    return tofxxf_write_reg(self, TOFXF_REG_RESTORE, 0xAA55);
}
