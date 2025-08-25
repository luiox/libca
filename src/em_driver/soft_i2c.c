#include "soft_i2c.h"
#include "vhil.h"

#define i2c_sda_out(soft_i2c) soft_i2c->gpio_set_output_mode(soft_i2c->sda)
#define i2c_sda_in(soft_i2c) soft_i2c->gpio_set_input_mode(soft_i2c->sda)
#define i2c_read_sda() (soft_i2c->gpio_read(soft_i2c->sda))
#define i2c_sda_high() soft_i2c->gpio_write(soft_i2c->sda, 1)
#define i2c_sda_low() soft_i2c->gpio_write(soft_i2c->sda, 0)
#define i2c_scl_high() soft_i2c->gpio_write(soft_i2c->scl, 1)
#define i2c_scl_low() soft_i2c->gpio_write(soft_i2c->scl, 0)
/*　
    下面的时间是通过安富莱AX-Pro逻辑分析仪测试得到的。
    CPU主频72MHz时，在内部Flash运行, MDK工程不优化
    循环次数为10时，SCL频率 = 205KHz
    循环次数为7时，SCL频率 = 347KHz， SCL高电平时间1.5us，SCL低电平时间2.87us
    循环次数为5时，SCL频率 = 421KHz， SCL高电平时间1.25us，SCL低电平时间2.375us

    IAR工程编译效率高，不能设置为7
        volatile uint8_t i;

    for (i = 0; i < 10; i++)
        ;
   */
#define soft_i2c_delay() soft_i2c->delay_us(4)


void soft_i2c_init(soft_i2c_t* soft_i2c);

/*
*********************************************************************************************************
*	函 数 名: i2c_Delay
*	功能说明: I2C总线位延迟，最快400KHz
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*	函 数 名: i2c_Start
*	功能说明: CPU发起I2C总线启动信号
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void soft_i2c_start(soft_i2c_t* soft_i2c)
{
    /*    _____
     *SDA      \_____________
     *    __________
     *SCL           \________
     */
    /* 当SCL高电平时，SDA出现一个下跳沿表示I2C总线启动信号 */
    i2c_sda_out(soft_i2c);
    i2c_sda_high();
    i2c_scl_high();
    soft_i2c_delay();
    i2c_sda_low();
    soft_i2c_delay();
    i2c_scl_low();
    soft_i2c_delay();
}

/*
*********************************************************************************************************
*	函 数 名: i2c_Start
*	功能说明: CPU发起I2C总线停止信号
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void soft_i2c_stop(soft_i2c_t* soft_i2c)
{

    /*               _______
     *SDA __________/
     *          ____________
     *SCL _____/
     */
    i2c_sda_out(soft_i2c);
    i2c_sda_low();
    i2c_scl_high();
    soft_i2c_delay();
    i2c_sda_high();
    soft_i2c_delay();
}

/*
*********************************************************************************************************
*	函 数 名: i2c_SendByte
*	功能说明: CPU向I2C总线设备发送8bit数据
*	形    参：_ucByte ： 等待发送的字节
*	返 回 值: 无
*********************************************************************************************************
*/
u8 soft_i2c_send_byte(soft_i2c_t* soft_i2c, uint8_t byte)
{
    uint8_t i;
    i2c_sda_out(soft_i2c);
    i2c_scl_low();
    /* 先发送字节的高位bit7 */
    for (i = 0; i < 8; i++) {
        if ((byte & 0x80) >> 7) {
            i2c_sda_high();
        }
        else {
            i2c_sda_low();
        }
        byte <<= 1;
        soft_i2c_delay();
        i2c_scl_high();
        soft_i2c_delay();
        i2c_scl_low();
        if (i == 7) {
            // 释放总线
            i2c_sda_high();
        }
        soft_i2c_delay();
    }
    return soft_i2c_wait_ack(soft_i2c);
}

/*
*********************************************************************************************************
*	函 数 名: i2c_ReadByte
*	功能说明: CPU从I2C总线设备读取8bit数据
*	形    参：无
*	返 回 值: 读到的数据
*********************************************************************************************************
*/
uint8_t soft_i2c_read_byte(soft_i2c_t* soft_i2c, uint8_t ack)
{
    uint8_t i;
    uint8_t data = 0;
    /* 读到第1个bit为数据的bit7 */
    for (i = 0; i < 8; i++) {
        data <<= 1;
        i2c_scl_high();
        soft_i2c_delay();
        if (i2c_read_sda()) {
            data++;
        }
        i2c_scl_low();
        soft_i2c_delay();
    }
    return data;
}

/*
*********************************************************************************************************
*	函 数 名: i2c_WaitAck
*	功能说明: CPU产生一个时钟，并读取器件的ACK应答信号
*	形    参：无
*	返 回 值: 返回0表示正确应答，1表示无器件响应
*********************************************************************************************************
*/
uint8_t soft_i2c_wait_ack(soft_i2c_t* soft_i2c)
{
    uint32_t timeout = 0;
    i2c_sda_high(); /* CPU释放SDA总线 */
    i2c_sda_in(soft_i2c);
    soft_i2c_delay();
    i2c_scl_high(); /* CPU驱动SCL = 1, 此时器件会返回ACK应答 */
    soft_i2c_delay();
    /* CPU读取SDA口线状态 */
    while (i2c_read_sda()) {
        timeout++;
        if (timeout > 100)   // 2000
        {
            soft_i2c_stop(soft_i2c);
            return 1;
        }
    }
    i2c_scl_low();
    soft_i2c_delay();
    return 0;
}

/*
*********************************************************************************************************
*	函 数 名: i2c_Ack
*	功能说明: CPU产生一个ACK信号
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void soft_i2c_ack(soft_i2c_t* soft_i2c)
{
    /*           ____
     *SCL ______/    \______
     *    ____         _____
     *SDA     \_______/
     */
    i2c_scl_low();
    i2c_sda_out(soft_i2c);
    i2c_sda_high();
    soft_i2c_delay();
    i2c_sda_low();
    soft_i2c_delay();
    i2c_scl_high();
    soft_i2c_delay();
    i2c_scl_low();
    soft_i2c_delay();
    i2c_sda_high();
}

/*
*********************************************************************************************************
*	函 数 名: i2c_NAck
*	功能说明: CPU产生1个NACK信号
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void soft_i2c_nack(soft_i2c_t* soft_i2c)
{
    /*           ____
     *SCL ______/    \______
     *    __________________
     *SDA
     */
    i2c_scl_low();
    i2c_sda_out(soft_i2c);
    i2c_sda_high();
    soft_i2c_delay();
    i2c_scl_high();
    soft_i2c_delay();
    i2c_scl_low();
    soft_i2c_delay();
}

/*
*********************************************************************************************************
*	函 数 名: i2c_GPIO_Config
*	功能说明: 配置I2C总线的GPIO，采用模拟IO的方式实现
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void soft_i2c_init(soft_i2c_t* soft_i2c)
{
    /* 给一个停止信号, 复位I2C总线上的所有设备到待机模式 */
    soft_i2c_stop(soft_i2c);
}

/*
*********************************************************************************************************
*	函 数 名: i2c_CheckDevice
*	功能说明: 检测I2C总线设备，CPU向发送设备地址，然后读取设备应答来判断该设备是否存在
*	形    参：_Address：设备的I2C总线地址
*	返 回 值: 返回值 0 表示正确， 返回1表示未探测到
*********************************************************************************************************
*/
uint8_t soft_i2c_check_device(soft_i2c_t* soft_i2c, uint8_t address)
{
    uint8_t ack;

    soft_i2c_init(soft_i2c); /* 配置GPIO */

    soft_i2c_start(soft_i2c); /* 发送启动信号 */

    /* 发送设备地址+读写控制bit（0 = w， 1 = r) bit7 先传 */
    soft_i2c_send_byte(soft_i2c, address | I2C_WRITE);
    ack = soft_i2c_wait_ack(soft_i2c); /* 检测设备的ACK应答 */

    soft_i2c_stop(soft_i2c); /* 发送停止信号 */

    return ack;
}

vhil_state_type_t soft_i2c_master_write(soft_i2c_t* soft_i2c, u16 dev_addr, u8* data, u16 data_size,
                                        u32 timeout)
{
    soft_i2c_start(soft_i2c);
    // 发送设备地址
    if (soft_i2c_send_byte(soft_i2c, dev_addr & 0xff) == 1) {
        // 无响应
        return VHAL_ERROR;
    }
    // 发送数据
    for (u16 i = 0; i < data_size; i++) {
        if (soft_i2c_send_byte(soft_i2c, data[i]) == 1) {
            // 无响应
            return VHAL_ERROR;
        }
    }
    // 停止
    soft_i2c_stop(soft_i2c);

    return VHAL_OK;
}
vhil_state_type_t soft_i2c_master_read(soft_i2c_t* soft_i2c, u16 dev_addr, u8* data, u16 data_size,
                                       u32 timeout)
{
    soft_i2c_start(soft_i2c);

    if (soft_i2c_send_byte(soft_i2c, dev_addr) == 1) {
        return VHAL_ERROR;
    }

    soft_i2c_start(soft_i2c);

    // 循环读数据
    for (u16 i = 0; i < data_size - 1; i++) {
        data[i] = soft_i2c_read_byte(soft_i2c, i == data_size - 1);
        // 发应答位
        soft_i2c_ack(soft_i2c);
    }
    // 接收最后一个
    data[data_size - 1] = soft_i2c_read_byte(soft_i2c, 1);
    // nack
    soft_i2c_nack(soft_i2c);

    soft_i2c_stop(soft_i2c);
    return VHAL_OK;
}
vhil_state_type_t soft_i2c_slave_write(soft_i2c_t* soft_i2c, u16 dev_addr, u8* data, u16 data_size,
                                       u32 timeout)
{
    return VHAL_OK;
}
vhil_state_type_t soft_i2c_slave_read(soft_i2c_t* soft_i2c, u16 dev_addr, u8* data, u16 data_size,
                                      u32 timeout)
{
    return VHAL_OK;
}
vhil_state_type_t soft_i2c_mem_write(soft_i2c_t* soft_i2c, u16 dev_addr, u16 mem_addr,
                                     u16 mem_addr_size, u8* data, u16 data_size, u32 timeout)
{
    soft_i2c_start(soft_i2c);
    // 发送设备地址
    if (soft_i2c_send_byte(soft_i2c, dev_addr & 0xff) == 1) {
        // 无响应
        return VHAL_ERROR;
    }
    // 发送寄存器地址
    if (data_size == I2C_MEM_ADDR_SIZE_8BIT) {
        if (soft_i2c_send_byte(soft_i2c, mem_addr & 0xff) == 1) {
            // 无响应
            return VHAL_ERROR;
        }
    }
    else {
        // 拆分为两个字节发，先发高位
        if (soft_i2c_send_byte(soft_i2c, (mem_addr >> 8) & 0xff) == 1) {
            // 无响应
            return VHAL_ERROR;
        }
        // 再发低位
        if (soft_i2c_send_byte(soft_i2c, mem_addr & 0xff) == 1) {
            // 无响应
            return VHAL_ERROR;
        }
    }
    // 发送数据
    for (u16 i = 0; i < data_size; i++) {
        if (soft_i2c_send_byte(soft_i2c, data[i]) == 1) {
            // 无响应
            return VHAL_ERROR;
        }
    }
    // 停止
    soft_i2c_stop(soft_i2c);

    return VHAL_OK;
}
vhil_state_type_t soft_i2c_mem_read(soft_i2c_t* soft_i2c, u16 dev_addr, u16 mem_addr,
                                    u16 mem_addr_size, u8* data, u16 data_size, u32 timeout)
{
    soft_i2c_start(soft_i2c);

    // 发设备地址
    if (soft_i2c_send_byte(soft_i2c, dev_addr) == 1) {
        return VHAL_ERROR;
    }
    // 发送寄存器地址
    if (data_size == I2C_MEM_ADDR_SIZE_8BIT) {
        if (soft_i2c_send_byte(soft_i2c, mem_addr & 0xff) == 1) {
            // 无响应
            return VHAL_ERROR;
        }
    }
    else {
        // 拆分为两个字节发，先发高位
        if (soft_i2c_send_byte(soft_i2c, (mem_addr >> 8) & 0xff) == 1) {
            // 无响应
            return VHAL_ERROR;
        }
        // 再发低位
        if (soft_i2c_send_byte(soft_i2c, mem_addr & 0xff) == 1) {
            // 无响应
            return VHAL_ERROR;
        }
    }

    soft_i2c_start(soft_i2c);

    // 循环读数据
    for (u16 i = 0; i < data_size - 1; i++) {
        data[i] = soft_i2c_read_byte(soft_i2c, i == data_size - 1);
        // 发应答位
        soft_i2c_ack(soft_i2c);
    }
    // 接收最后一个
    data[data_size - 1] = soft_i2c_read_byte(soft_i2c, 1);
    // nack
    soft_i2c_nack(soft_i2c);

    soft_i2c_stop(soft_i2c);
    return VHAL_OK;
}

vhil_state_type_t soft_i2c_is_device_ready(soft_i2c_t* soft_i2c, u16 dev_addr, u32 trials,
                                           u32 timeout)
{
    uint8_t ack;
    uint32_t trial_cnt = 0;
    
    if (trials == 0) {
        return VHAL_ERROR;
    }
    
    do {
        soft_i2c_init(soft_i2c); /* 初始化I2C总线 */
        soft_i2c_start(soft_i2c); /* 发送启动信号 */
        
        /* 发送设备地址+写控制bit */
        soft_i2c_send_byte(soft_i2c, dev_addr & 0xFE); /* 确保最低位为0（写操作） */
        ack = soft_i2c_wait_ack(soft_i2c); /* 检测设备的ACK应答 */
        
        soft_i2c_stop(soft_i2c); /* 发送停止信号 */
        
        if (ack == 0) {
            return VHAL_OK; /* 设备响应，返回成功 */
        }
        
        trial_cnt++;
    } while (trial_cnt < trials);
    
    return VHAL_ERROR; /* 设备未响应，返回错误 */
}
