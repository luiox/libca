/**
 * @file nrf24.h
 * @author canrad (1517807724@qq.com)
 * @brief NRF24L01 2.4G无线模块
 * 参考文章：https://blog.csdn.net/weixin_43772810/article/details/123811245
 * 可以参考的代码：https://github.com/AFeng-Studio/NRF24L01plus_test/blob/main/Core/Src/NRF24L01.c
 * @version 0.1
 * @date 2026-01-22
 *
 * @copyright Copyright (c) 2026
 *
 */
#ifndef LIBCA_EM_DRIVER_NRF24_H
#define LIBCA_EM_DRIVER_NRF24_H

#include <em_base/datatype.h>

// port
typedef struct nrf24_port
{
    void (*write_pin)(void* gpio, u16 pin, u8 value);
    u8 (*read_pin)(void* gpio, u16 pin);
    void (*set_output_mode)(void* gpio, u16 pin);
    void (*set_input_mode)(void* gpio, u16 pin);
    void (*delay_us)(u32 us);
    void (*delay_ms)(u32 ms);
    /**
     * SPI 发送/接收原语。
     *
     * 参数：
     *  - hspi: 平台提供的不透明 SPI 句柄（使用 void*，避免依赖具体 HAL 类型）
     *  - data: 要通过 MOSI 发送的字节
     *
     * 返回值：本次全双工传输期间从 MISO 接收的字节。
     *
     * 要求/注意：
     *  - 必须执行一次阻塞的 8 位全双工传输并返回接收到的字节。
     *  - 本函数不得管理 CSN/CE 引脚；CSN/CE 由驱动通过独立端口函数控制。
     *  - 若 SPI 在多任务/中断环境中共享，实现应保证线程安全或由调用方做好保护。
     */
    u8 (*spi_send_recv)(void* hspi, u8 data);
} nrf24_port_t;

void nrf24_bind_port(const nrf24_port_t* port);
bool nrf24_port_is_registered(void);

// nRF24L01 data rate
typedef enum
{
    nRF24_DataRate_250kbps = (u8)0x20,   // 250kbps data rate
    nRF24_DataRate_1Mbps   = (u8)0x00,   // 1Mbps data rate
    nRF24_DataRate_2Mbps   = (u8)0x08    // 2Mbps data rate
} nRF24_DataRate_TypeDef;

// nRF24L01 RF output power in TX mode
typedef enum
{
    nRF24_TXPower_18dBm = (u8)0x00,   // -18dBm
    nRF24_TXPower_12dBm = (u8)0x02,   // -12dBm
    nRF24_TXPower_6dBm  = (u8)0x04,   //  -6dBm
    nRF24_TXPower_0dBm  = (u8)0x06    //   0dBm
} nRF24_TXPower_TypeDef;

// nRF24L01 CRC encoding scheme
typedef enum
{
    nRF24_CRC_off   = (u8)0x00,   // CRC disabled
    nRF24_CRC_1byte = (u8)0x08,   // 1-byte CRC
    nRF24_CRC_2byte = (u8)0x0c    // 2-byte CRC
} nRF24_CRC_TypeDef;

// nRF24L01 power control
typedef enum
{
    nRF24_PWR_Up   = (u8)0x02,   // Power up
    nRF24_PWR_Down = (u8)0x00    // Power down
} nRF24_PWR_TypeDef;

// nRF24L01 RX/TX control
typedef enum
{
    nRF24_PRIM_RX = (u8)0x01,   // PRX
    nRF24_PRIM_TX = (u8)0x00    // PTX
} nRF24_PRIM_TypeDef;

// RX data pipe
typedef enum
{
    nRF24_RX_PIPE0 = (u8)0x00,
    nRF24_RX_PIPE1 = (u8)0x01,
    nRF24_RX_PIPE2 = (u8)0x02,
    nRF24_RX_PIPE3 = (u8)0x03,
    nRF24_RX_PIPE4 = (u8)0x04,
    nRF24_RX_PIPE5 = (u8)0x05
} nRF24_RX_PIPE_TypeDef;

// nRF24L01 enable auto acknowledgment
typedef enum
{
    nRF24_ENAA_OFF = (u8)0x00,   // Disable auto acknowledgment
    nRF24_ENAA_P0  = (u8)0x01,   // Enable auto acknowledgment for PIPE#0
    nRF24_ENAA_P1  = (u8)0x02,   // Enable auto acknowledgment for PIPE#1
    nRF24_ENAA_P2  = (u8)0x04,   // Enable auto acknowledgment for PIPE#2
    nRF24_ENAA_P3  = (u8)0x08,   // Enable auto acknowledgment for PIPE#3
    nRF24_ENAA_P4  = (u8)0x10,   // Enable auto acknowledgment for PIPE#4
    nRF24_ENAA_P5  = (u8)0x20,   // Enable auto acknowledgment for PIPE#5
} nRF24_ENAA_TypeDef;

// RX packet pipe
typedef enum
{
    nRF24_RX_PCKT_PIPE0 = (u8)0x00,
    nRF24_RX_PCKT_PIPE1 = (u8)0x01,
    nRF24_RX_PCKT_PIPE2 = (u8)0x02,
    nRF24_RX_PCKT_PIPE3 = (u8)0x03,
    nRF24_RX_PCKT_PIPE4 = (u8)0x04,
    nRF24_RX_PCKT_PIPE5 = (u8)0x05,
    nRF24_RX_PCKT_EMPTY = (u8)0xfe,
    nRF24_RX_PCKT_ERROR = (u8)0xff
} nRF24_RX_PCKT_TypeDef;

// TX packet result
typedef enum
{
    nRF24_TX_SUCCESS,   // Packet transmitted successfully
    nRF24_TX_TIMEOUT,   // It was timeout during packet transmit
    nRF24_TX_MAXRT,     // Transmit failed with maximum auto retransmit count
    nRF24_TX_ERROR      // Some error happens
} nRF24_TX_PCKT_TypeDef;

// device object
typedef struct nrf24
{
    void* gpio;
    u16   csn_pin;
    u16   ce_pin;
    u16   irq_pin;
    void* spi;
} nrf24_t;

// API
void nrf24_init(nrf24_t* self, void* gpio, u16 csn_pin, u16 ce_pin, void* spi, u16 irq_pin);

void nrf24_write_reg(nrf24_t* self, u8 reg, u8 value);
u8   nrf24_read_reg(nrf24_t* self, u8 reg);
void nrf24_read_buf(nrf24_t* self, u8 reg, u8* pBuf, u8 count);
void nrf24_write_buf(nrf24_t* self, u8 reg, u8* pBuf, u8 count);

u8 nrf24_check(nrf24_t* self);

void nrf24_set_rf_channel(nrf24_t* self, u8 RFChannel);
void nrf24_flush_tx(nrf24_t* self);
void nrf24_flush_rx(nrf24_t* self);
void nrf24_tx_mode(nrf24_t* self, u8 RetrCnt, u8 RetrDelay, u8 RFChan,
                   nRF24_DataRate_TypeDef DataRate, nRF24_TXPower_TypeDef TXPower,
                   nRF24_CRC_TypeDef CRCS, nRF24_PWR_TypeDef Power, u8* TX_Addr, u8 TX_Addr_Width);
void nrf24_rx_mode(nrf24_t* self, nRF24_RX_PIPE_TypeDef PIPE, nRF24_ENAA_TypeDef PIPE_AA, u8 RFChan,
                   nRF24_DataRate_TypeDef DataRate, nRF24_CRC_TypeDef CRCS, u8* RX_Addr,
                   u8 RX_Addr_Width, u8 RX_PAYLOAD, nRF24_TXPower_TypeDef TXPower);
void nrf24_set_pipe_addr(nrf24_t* self, nRF24_RX_PIPE_TypeDef PIPE, u8* Addr, u8 Addr_Width);

nRF24_TX_PCKT_TypeDef nrf24_tx_packet(nrf24_t* self, u8* pBuf, u8 TX_PAYLOAD);
nRF24_RX_PCKT_TypeDef nrf24_rx_packet(nrf24_t* self, u8* pBuf, u8 RX_PAYLOAD);

void nrf24_clear_irq_flags(nrf24_t* self);
void nrf24_power_down(nrf24_t* self);
void nrf24_wake(nrf24_t* self);
void nrf24_set_tx_power(nrf24_t* self, nRF24_TXPower_TypeDef TXPower);

#endif   // LIBCA_EM_DRIVER_NRF24_H
