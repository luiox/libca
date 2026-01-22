#include "nrf24.h"
#include <string.h>

// Internal commands/registers/masks (implementation detail)
#define nRF24_CMD_RREG 0x00
#define nRF24_CMD_WREG 0x20
#define nRF24_CMD_R_RX_PAYLOAD 0x61
#define nRF24_CMD_W_TX_PAYLOAD 0xA0
#define nRF24_CMD_FLUSH_TX 0xE1
#define nRF24_CMD_FLUSH_RX 0xE2
#define nRF24_CMD_REUSE_TX_PL 0xE3
#define nRF24_CMD_NOP 0xFF

#define nRF24_REG_CONFIG 0x00
#define nRF24_REG_EN_AA 0x01
#define nRF24_REG_EN_RXADDR 0x02
#define nRF24_REG_SETUP_AW 0x03
#define nRF24_REG_SETUP_RETR 0x04
#define nRF24_REG_RF_CH 0x05
#define nRF24_REG_RF_SETUP 0x06
#define nRF24_REG_STATUS 0x07
#define nRF24_REG_OBSERVE_TX 0x08
#define nRF24_REG_CD 0x09
#define nRF24_REG_RX_ADDR_P0 0x0A
#define nRF24_REG_RX_ADDR_P1 0x0B
#define nRF24_REG_RX_ADDR_P2 0x0C
#define nRF24_REG_RX_ADDR_P3 0x0D
#define nRF24_REG_RX_ADDR_P4 0x0E
#define nRF24_REG_RX_ADDR_P5 0x0F
#define nRF24_REG_TX_ADDR 0x10
#define nRF24_REG_RX_PW_P0 0x11
#define nRF24_REG_RX_PW_P1 0x12
#define nRF24_REG_RX_PW_P2 0x13
#define nRF24_REG_RX_PW_P3 0x14
#define nRF24_REG_RX_PW_P4 0x15
#define nRF24_REG_RX_PW_P5 0x16
#define nRF24_REG_FIFO_STATUS 0x17
#define nRF24_REG_DYNPD 0x1C
#define nRF24_REG_FEATURE 0x1D

#define nRF24_MASK_RX_DR 0x40
#define nRF24_MASK_TX_DS 0x20
#define nRF24_MASK_MAX_RT 0x10
#define nRF24_FIFO_RX_EMPTY 0x01
#define nRF24_FIFO_RX_FULL 0x02

#define nRF24_TEST_ADDR "nRF24"
#define nRF24_WAIT_TIMEOUT 0x000FFFFF

static const u8 RX_PW_PIPES[6]   = {nRF24_REG_RX_PW_P0,
                                    nRF24_REG_RX_PW_P1,
                                    nRF24_REG_RX_PW_P2,
                                    nRF24_REG_RX_PW_P3,
                                    nRF24_REG_RX_PW_P4,
                                    nRF24_REG_RX_PW_P5};
static const u8 RX_ADDR_PIPES[6] = {nRF24_REG_RX_ADDR_P0,
                                    nRF24_REG_RX_ADDR_P1,
                                    nRF24_REG_RX_ADDR_P2,
                                    nRF24_REG_RX_ADDR_P3,
                                    nRF24_REG_RX_ADDR_P4,
                                    nRF24_REG_RX_ADDR_P5};


static const nrf24_port_t* g_nrf24_port = NULL;

void nrf24_bind_port(const nrf24_port_t* port)
{
    g_nrf24_port = port;
}

bool nrf24_port_is_registered(void)
{
    return g_nrf24_port != NULL;
}

// Initialize driver object (bind gpio/pins/spi)
void nrf24_init(nrf24_t* self, void* gpio, u16 csn_pin, u16 ce_pin, void* spi, u16 irq_pin)
{
    if (!self)
        return;
    self->gpio    = gpio;
    self->csn_pin = csn_pin;
    self->ce_pin  = ce_pin;
    self->irq_pin = irq_pin;
    self->spi     = spi;

    if (!g_nrf24_port)
        return;   // port must be registered

    g_nrf24_port->set_output_mode(self->gpio, self->csn_pin);
    g_nrf24_port->set_output_mode(self->gpio, self->ce_pin);
    g_nrf24_port->set_input_mode(self->gpio, self->irq_pin);

    g_nrf24_port->write_pin(self->gpio, self->csn_pin, 1);   // chip release
    g_nrf24_port->write_pin(self->gpio, self->ce_pin, 0);    // RX/TX disable

    nrf24_flush_rx(self);
    nrf24_flush_tx(self);
    nrf24_clear_irq_flags(self);
}

// Write new value to register
// input:
//   reg - register number
//   value - new value
void nrf24_write_reg(nrf24_t* self, u8 reg, u8 value)
{
    if (!g_nrf24_port || !self)
        return;
    g_nrf24_port->write_pin(self->gpio, self->csn_pin, 0);
    g_nrf24_port->spi_send_recv(self->spi, reg);
    g_nrf24_port->spi_send_recv(self->spi, value);
    g_nrf24_port->write_pin(self->gpio, self->csn_pin, 1);
}

// Read nRF24L01 register
// input:
//   reg - register number
// return: register value
u8 nrf24_read_reg(nrf24_t* self, u8 reg)
{
    u8 value = 0;
    if (!g_nrf24_port || !self)
        return value;

    g_nrf24_port->write_pin(self->gpio, self->csn_pin, 0);
    g_nrf24_port->spi_send_recv(self->spi, reg & 0x1f);              // Select register to read from
    value = g_nrf24_port->spi_send_recv(self->spi, nRF24_CMD_NOP);   // Read register value
    g_nrf24_port->write_pin(self->gpio, self->csn_pin, 1);

    return value;
}

// Get data from nRF24L01 into buffer
// input:
//   reg - register number
//   pBuf - pointer to buffer
//   count - bytes count
void nrf24_read_buf(nrf24_t* self, u8 reg, u8* pBuf, u8 count)
{
    if (!g_nrf24_port || !self)
        return;
    g_nrf24_port->write_pin(self->gpio, self->csn_pin, 0);
    g_nrf24_port->spi_send_recv(self->spi, reg);   // Select register to read from
    while (count--)
        *pBuf++ = g_nrf24_port->spi_send_recv(self->spi, nRF24_CMD_NOP);
    g_nrf24_port->write_pin(self->gpio, self->csn_pin, 1);
}

// Send buffer to nRF24L01
// input:
//   reg - register number
//   pBuf - pointer to buffer
//   count - bytes count
void nrf24_write_buf(nrf24_t* self, u8 reg, u8* pBuf, u8 count)
{
    if (!g_nrf24_port || !self)
        return;
    g_nrf24_port->write_pin(self->gpio, self->csn_pin, 0);
    g_nrf24_port->spi_send_recv(self->spi, reg);   // Select register
    while (count--)
        g_nrf24_port->spi_send_recv(self->spi, *pBuf++);
    g_nrf24_port->write_pin(self->gpio, self->csn_pin, 1);
}

// Check if nRF24L01 present (send byte sequence, read it back and compare)
// return:
//   1 - nRF24L01 is online and responding
//   0 - received sequence differs from original
u8 nrf24_check(nrf24_t* self)
{
    if (!g_nrf24_port || !self)
        return 0;

    u8  rxbuf[5];
    u8* ptr = (u8*)nRF24_TEST_ADDR;
    u8  i;

    nrf24_write_buf(self, nRF24_CMD_WREG | nRF24_REG_TX_ADDR, ptr, 5);   // Write fake TX address
    nrf24_read_buf(self, nRF24_REG_TX_ADDR, rxbuf, 5);                   // Read TX_ADDR register
    for (i = 0; i < 5; i++)
        if (rxbuf[i] != *ptr++)
            return 0;

    return 1;
}

// Set nRF24L01 frequency channel
// input:
//   RFChannel - Frequency channel (0..127) (frequency = 2400 + RFChan [MHz])
// Note, what part of the OBSERVER_TX register called "PLOS_CNT" will be cleared!
void nrf24_set_rf_channel(nrf24_t* self, u8 RFChannel)
{
    nrf24_write_reg(self, nRF24_CMD_WREG | nRF24_REG_RF_CH, RFChannel);
}

// Flush nRF24L01 TX FIFO buffer
void nrf24_flush_tx(nrf24_t* self)
{
    nrf24_write_reg(self, nRF24_CMD_FLUSH_TX, 0xFF);
}

// Flush nRF24L01 RX FIFO buffer
void nrf24_flush_rx(nrf24_t* self)
{
    nrf24_write_reg(self, nRF24_CMD_FLUSH_RX, 0xFF);
}

// Put nRF24L01 in TX mode
// input:
//   RetrCnt - Auto retransmit count on fail of AA (1..15 or 0 for disable)
//   RetrDelay - Auto retransmit delay 250us+(0..15)*250us (0 = 250us, 15 = 4000us)
//   RFChan - Frequency channel (0..127) (frequency = 2400 + RFChan [MHz])
//   DataRate - Set data rate: nRF24_DataRate_1Mbps or nRF24_DataRate_2Mbps
//   TXPower - RF output power (-18dBm, -12dBm, -6dBm, 0dBm)
//   CRCS - CRC encoding scheme (nRF24_CRC_[off | 1byte | 2byte])
//   PWR - power state (nRF24_PWR_Up or nRF24_PWR_Down)
//   TX_Addr - buffer with TX address
//   TX_Addr_Width - size of the TX address (3..5 bytes)
void nrf24_tx_mode(nrf24_t* self, u8 RetrCnt, u8 RetrDelay, u8 RFChan,
                   nRF24_DataRate_TypeDef DataRate, nRF24_TXPower_TypeDef TXPower,
                   nRF24_CRC_TypeDef CRCS, nRF24_PWR_TypeDef Power, u8* TX_Addr, u8 TX_Addr_Width)
{
    if (!g_nrf24_port || !self)
        return;

    g_nrf24_port->write_pin(self->gpio, self->ce_pin, 0);
    nrf24_write_reg(self,
                    nRF24_CMD_WREG | nRF24_REG_SETUP_RETR,
                    ((RetrDelay << 4) & 0xf0) | (RetrCnt & 0x0f));   // Auto retransmit settings
    nrf24_write_reg(
        self, nRF24_CMD_WREG | nRF24_REG_RF_SETUP, (u8)DataRate | (u8)TXPower);   // Setup register
    nrf24_write_reg(self,
                    nRF24_CMD_WREG | nRF24_REG_CONFIG,
                    (u8)CRCS | (u8)Power | nRF24_PRIM_TX);   // Config register
    nrf24_set_rf_channel(self, RFChan);                      // Set frequency channel
    nrf24_write_reg(self,
                    nRF24_CMD_WREG | nRF24_REG_EN_AA,
                    0x01);   // Enable ShockBurst for data pipe 0 to receive ACK packet
    nrf24_write_reg(
        self, nRF24_CMD_WREG | nRF24_REG_SETUP_AW, TX_Addr_Width);   // Set address width
    nrf24_write_buf(self,
                    nRF24_CMD_WREG | nRF24_REG_TX_ADDR,
                    TX_Addr,
                    TX_Addr_Width);   // Set static TX address
    nrf24_write_buf(
        self,
        nRF24_CMD_WREG | nRF24_REG_RX_ADDR_P0,
        TX_Addr,
        TX_Addr_Width);   // Static RX address on PIPE0 must same as TX address for auto ACK
}

// Put nRF24L01 in RX mode
// input:
//   PIPE - RX data pipe (nRF24_RX_PIPE[0..5])
//   PIPE_AA - auto acknowledgment for data pipe (nRF24_ENAA_P[0..5] or nRF24_ENAA_OFF)
//   RFChan - Frequency channel (0..127) (frequency = 2400 + RFChan [MHz])
//   DataRate - Set data rate (nRF24_DataRate_[250kbps,1Mbps,2Mbps])
//   CRCS - CRC encoding scheme (nRF24_CRC_[off | 1byte | 2byte])
//   RX_Addr - buffer with TX address
//   RX_Addr_Width - size of TX address (3..5 byte)
//   RX_PAYLOAD - receive buffer length
//   TXPower - RF output power for ACK packets (-18dBm, -12dBm, -6dBm, 0dBm)
void nrf24_rx_mode(nrf24_t* self, nRF24_RX_PIPE_TypeDef PIPE, nRF24_ENAA_TypeDef PIPE_AA, u8 RFChan,
                   nRF24_DataRate_TypeDef DataRate, nRF24_CRC_TypeDef CRCS, u8* RX_Addr,
                   u8 RX_Addr_Width, u8 RX_PAYLOAD, nRF24_TXPower_TypeDef TXPower)
{
    if (!g_nrf24_port || !self)
        return;

    g_nrf24_port->write_pin(self->gpio, self->ce_pin, 0);
    nrf24_read_reg(self, nRF24_CMD_NOP);   // Dummy read
    u8 rreg = nrf24_read_reg(self, nRF24_REG_EN_AA);
    if (PIPE_AA != nRF24_ENAA_OFF) {
        // Enable auto acknowledgment for given data pipe
        rreg |= (u8)PIPE_AA;
    }
    else {
        // Disable auto acknowledgment for given data pipe
        rreg &= ~(1 << (u8)PIPE);
    }
    nrf24_write_reg(self, nRF24_CMD_WREG | nRF24_REG_EN_AA, rreg);
    rreg = nrf24_read_reg(self, nRF24_REG_EN_RXADDR);
    nrf24_write_reg(self,
                    nRF24_CMD_WREG | nRF24_REG_EN_RXADDR,
                    rreg | (1 << (u8)PIPE));   // Enable given data pipe
    nrf24_write_reg(
        self, nRF24_CMD_WREG | RX_PW_PIPES[(u8)PIPE], RX_PAYLOAD);   // Set RX payload length
    nrf24_write_reg(
        self, nRF24_CMD_WREG | nRF24_REG_RF_SETUP, (u8)DataRate | (u8)TXPower);   // SETUP register
    nrf24_write_reg(self,
                    nRF24_CMD_WREG | nRF24_REG_CONFIG,
                    (u8)CRCS | nRF24_PWR_Up | nRF24_PRIM_RX);   // Config register
    nrf24_set_rf_channel(self, RFChan);                         // Set frequency channel
    nrf24_write_reg(self,
                    nRF24_CMD_WREG | nRF24_REG_SETUP_AW,
                    RX_Addr_Width - 2);   // Set of address widths (common for all data pipes)
    nrf24_write_buf(self,
                    nRF24_CMD_WREG | RX_ADDR_PIPES[(u8)PIPE],
                    RX_Addr,
                    RX_Addr_Width);   // Set static RX address for given data pipe
    nrf24_clear_irq_flags(self);
    nrf24_flush_rx(self);
    g_nrf24_port->write_pin(self->gpio, self->ce_pin, 1);   // RX mode
}

// Send data packet
// input:
//   pBuf - buffer with data to send
//   TX_PAYLOAD - buffer size
// return:
//   nRF24_TX_XXX values
nRF24_TX_PCKT_TypeDef nrf24_tx_packet(nrf24_t* self, u8* pBuf, u8 TX_PAYLOAD)
{
    if (!g_nrf24_port || !self)
        return nRF24_TX_ERROR;
    u8  status;
    u32 wait = nRF24_WAIT_TIMEOUT;

    // Release CE pin (in case if it still high)
    g_nrf24_port->write_pin(self->gpio, self->ce_pin, 0);
    // Transfer data from specified buffer to the TX FIFO
    nrf24_write_buf(self, nRF24_CMD_W_TX_PAYLOAD, pBuf, TX_PAYLOAD);
    // CE pin high => Start transmit (must hold pin at least 10us)
    g_nrf24_port->write_pin(self->gpio, self->ce_pin, 1);
    // Wait for an IRQ from nRF24L01 (IRQ pin low when asserted)
    while (g_nrf24_port->read_pin(self->gpio, self->irq_pin) && --wait) {}
    if (!wait)
        return nRF24_TX_TIMEOUT;
    // Release CE pin
    g_nrf24_port->write_pin(self->gpio, self->ce_pin, 0);

    // Read the status register
    status = nrf24_read_reg(self, nRF24_REG_STATUS);
    // Clear pending IRQ flags
    nrf24_write_reg(self, nRF24_CMD_WREG | nRF24_REG_STATUS, status | 0x70);
    if (status & nRF24_MASK_MAX_RT) {
        // Auto retransmit counter exceeds the programmed maximum limit. FIFO is not removed.
        nrf24_flush_tx(self);
        return nRF24_TX_MAXRT;
    }
    if (status & nRF24_MASK_TX_DS) {
        // Transmit successful
        return nRF24_TX_SUCCESS;
    }

    // Some unexpected state
    nrf24_flush_tx(self);
    nrf24_clear_irq_flags(self);

    return nRF24_TX_ERROR;
}

// Receive data packet
// input:
//   pBuf - buffer for received data
//   RX_PAYLOAD - buffer size
// return:
//   nRF24_RX_PCKT_PIPE[0..5] - packet received from specific data pipe
//   nRF24_RX_PCKT_ERROR - RX_DR bit was not set
//   nRF24_RX_PCKT_EMPTY - RX FIFO is empty
nRF24_RX_PCKT_TypeDef nrf24_rx_packet(nrf24_t* self, u8* pBuf, u8 RX_PAYLOAD)
{
    if (!g_nrf24_port || !self)
        return nRF24_RX_PCKT_ERROR;
    u8                    status;
    nRF24_RX_PCKT_TypeDef result = nRF24_RX_PCKT_ERROR;

    status = nrf24_read_reg(self, nRF24_REG_STATUS);   // Read the status register
    if (status & nRF24_MASK_RX_DR) {
        // RX_DR bit set (Data ready RX FIFO interrupt)
        result = (nRF24_RX_PCKT_TypeDef)((status & 0x0e) >> 1);   // Pipe number
        if ((u8)result < 6) {
            // Read received payload from RX FIFO buffer
            nrf24_read_buf(self, nRF24_CMD_R_RX_PAYLOAD, pBuf, RX_PAYLOAD);
            // Clear pending IRQ flags
            nrf24_write_reg(self, nRF24_CMD_WREG | nRF24_REG_STATUS, status | 0x70);
            // Check if RX FIFO is empty and flush it if not
            status = nrf24_read_reg(self, nRF24_REG_FIFO_STATUS);
            if (status & nRF24_FIFO_RX_EMPTY)
                nrf24_flush_rx(self);

            return result;   // Data pipe number
        }
        else {
            // RX FIFO is empty
            return nRF24_RX_PCKT_EMPTY;
        }
    }

    // Some unexpected state
    nrf24_flush_rx(self);   // Flush the RX FIFO buffer
    nrf24_clear_irq_flags(self);

    return result;
}

// Clear pending IRQ flags
void nrf24_clear_irq_flags(nrf24_t* self)
{
    if (!g_nrf24_port || !self)
        return;
    u8 status = nrf24_read_reg(self, nRF24_REG_STATUS);
    nrf24_write_reg(self, nRF24_CMD_WREG | nRF24_REG_STATUS, status | 0x70);
}

// Put nRF24 in Power Down mode
void nrf24_power_down(nrf24_t* self)
{
    if (!g_nrf24_port || !self)
        return;
    g_nrf24_port->write_pin(self->gpio, self->ce_pin, 0);   // CE pin to low
    u8 conf = nrf24_read_reg(self, nRF24_REG_CONFIG);
    conf &= ~(1 << 1);                                                // Clear PWR_UP bit
    nrf24_write_reg(self, nRF24_CMD_WREG | nRF24_REG_CONFIG, conf);   // Go Power down mode
}

// Wake nRF24 from Power Down mode
// note: with external crystal it wake to Standby-I mode within 1.5ms
void nrf24_wake(nrf24_t* self)
{
    if (!g_nrf24_port || !self)
        return;
    u8 conf = nrf24_read_reg(self, nRF24_REG_CONFIG) | (1 << 1);      // Set PWR_UP bit
    nrf24_write_reg(self, nRF24_CMD_WREG | nRF24_REG_CONFIG, conf);   // Wake-up
}

// Configure RF output power in TX mode
// input:
//   TXPower - RF output power (-18dBm, -12dBm, -6dBm, 0dBm)
void nrf24_set_tx_power(nrf24_t* self, nRF24_TXPower_TypeDef TXPower)
{
    if (!g_nrf24_port || !self)
        return;
    u8 rf_setup = nrf24_read_reg(self, nRF24_REG_RF_SETUP);
    rf_setup &= 0xf9;   // Clear RF_PWR bits
    nrf24_write_reg(self, nRF24_CMD_WREG | nRF24_REG_RF_SETUP, rf_setup | (u8)TXPower);
}
