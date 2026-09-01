#include "w25qxx.h"

// 指令表
#define W25QXX_WriteEnable 0x06
#define W25QXX_WriteDisable 0x04
#define W25QXX_ReadStatusReg1 0x05
#define W25QXX_ReadStatusReg2 0x35
#define W25QXX_ReadStatusReg3 0x15
#define W25QXX_WriteStatusReg1 0x01
#define W25QXX_WriteStatusReg2 0x31
#define W25QXX_WriteStatusReg3 0x11
#define W25QXX_ReadData 0x03
#define W25QXX_FastReadData 0x0B
#define W25QXX_FastReadDual 0x3B
#define W25QXX_PageProgram 0x02
#define W25QXX_BlockErase 0xD8
#define W25QXX_SectorErase 0x20
#define W25QXX_ChipErase 0xC7
#define W25QXX_PowerDown 0xB9
#define W25QXX_ReleasePowerDown 0xAB
#define W25QXX_DeviceID 0xAB
#define W25QXX_ManufactDeviceID 0x90
#define W25QXX_JedecDeviceID 0x9F
#define W25QXX_Enable4ByteAddr 0xB7
#define W25QXX_Exit4ByteAddr 0xE9

// W25X系列/Q系列芯片列表
#define W25Q80 0XEF13
#define W25Q16 0XEF14
#define W25Q32 0XEF15
#define W25Q64 0XEF16
#define W25Q128 0XEF17
#define W25Q256 0XEF18

////////////////////////////////////////////////////////////////////////////////

#if (LIBCA_W25QXX_PORT_MODE == LIBCA_W25QXX_PORT_MODE_EXTERN)
#    define W25QXX_WRITE_PIN(gpio_port, pin, value) \
        port_w25qxx_write_pin((gpio_port), (pin), (value))
#    define W25QXX_SPI_TRANSMIT(hspi, data, size, timeout) \
        port_w25qxx_spi_transmit((hspi), (data), (size), (timeout))
#    define W25QXX_SPI_RECEIVE(hspi, data, size, timeout) \
        port_w25qxx_spi_receive((hspi), (data), (size), (timeout))
#    define W25QXX_SPI_TXRX(hspi, tx_data, rx_data, size, to) \
        port_w25qxx_spi_transmit_receive((hspi), (tx_data), (rx_data), (size), (to))

#elif (LIBCA_W25QXX_PORT_MODE == LIBCA_W25QXX_PORT_MODE_DYNAMIC)
static const w25qxx_port_t* g_w25qxx_port = NULL;
#    define W25QXX_WRITE_PIN(gpio_port, pin, value) \
        g_w25qxx_port->write_pin((gpio_port), (pin), (value))
#    define W25QXX_SPI_TRANSMIT(hspi, data, size, timeout) \
        g_w25qxx_port->spi_transmit((hspi), (data), (size), (timeout))
#    define W25QXX_SPI_RECEIVE(hspi, data, size, timeout) \
        g_w25qxx_port->spi_receive((hspi), (data), (size), (timeout))
#    define W25QXX_SPI_TXRX(hspi, tx_data, rx_data, size, to) \
        g_w25qxx_port->spi_transmit_receive((hspi), (tx_data), (rx_data), (size), (to))

#else
#    error "Invalid W25QXX port mode"
#endif

#if (LIBCA_W25QXX_PORT_MODE == LIBCA_W25QXX_PORT_MODE_DYNAMIC)

void w25qxx_bind_port(const w25qxx_port_t* port)
{
    g_w25qxx_port = port;
}

bool w25qxx_port_is_registered(void)
{
    return g_w25qxx_port != NULL;
}
#endif

////////////////////////////////////////////////////////////////////////////////

#define W25QXX_CS_LOW() W25QXX_WRITE_PIN(self->cs_gpio, self->cs_pin, 0)
#define W25QXX_CS_HIGH() W25QXX_WRITE_PIN(self->cs_gpio, self->cs_pin, 1)

// 使用之前需要初始化好SPI和CS引脚，这个函数负责让模块绑定SPI和CS引脚
void w25qxx_init(w25qxx_t* self)
{
    // nothing to do
}

// 发送一个字节并接收返回（SPI全双工同时收发）
int8_t w25qxx_send_byte(w25qxx_t* self, u8 byte)
{
    u8 ret = 0;

    // 使用全双工收发
    W25QXX_SPI_TXRX(self->hspi, &byte, &ret, 1, 1000);
    return ret;
}

// 读取设备ID
u16 w25qxx_read_device_id(w25qxx_t* self)
{
    u16 ID = 0;
    // 1.把CS片选引脚拉低  表示选中
    W25QXX_CS_LOW();

    // 2.发送指令
    w25qxx_send_byte(self, 0x90);

    // 3.发送地址  24bit
    w25qxx_send_byte(self, 0x00);
    w25qxx_send_byte(self, 0x00);
    w25qxx_send_byte(self, 0x00);

    // 4.接收厂商ID和设备ID  主机发送任意数据即可
    ID = w25qxx_send_byte(self, 0x00) << 8;
    ID |= w25qxx_send_byte(self, 0x00);

    // 5.把CS片选引脚拉高  表示不通信
    W25QXX_CS_HIGH();

    return ID;
}

// 使能写入
void w25qxx_write_enable(w25qxx_t* self)
{

    // 1.把CS片选引脚拉低  表示选中
    W25QXX_CS_LOW();

    // 2.发送指令(0x06是写使能)
    w25qxx_send_byte(self, 0x06);

    // 3.把CS片选引脚拉高  表示不通信
    W25QXX_CS_HIGH();
}
// 禁用写入
void w25qxx4_write_disable(w25qxx_t* self)
{
    // 1.把CS片选引脚拉低  表示选中
    W25QXX_CS_LOW();

    // 2.发送指令(0x04是写禁用)
    w25qxx_send_byte(self, 0x04);

    // 3.把CS片选引脚拉高  表示不通信
    W25QXX_CS_HIGH();
}
// 读取状态寄存器
u8 w25qxx_read_status_register(w25qxx_t* self)
{
    u8 status = 0;
    // 1.把CS片选引脚拉低  表示选中
    W25QXX_CS_LOW();

    // 2.发送指令
    w25qxx_send_byte(self, 0x05);

    // 3.发送任意数据  目的是接收一个字节的返回值
    status = w25qxx_send_byte(self, 0xFF);

    // 4.把CS片选引脚拉高  表示不通信
    W25QXX_CS_HIGH();

    return status;
}

// 擦除扇区
void w25qxx_sector_erase(w25qxx_t* self, u32 address)
{
    // 1.开启写使能
    w25qxx_write_enable(self);
    // systick_delay_ms(1);

    // 2.把CS片选引脚拉低  表示选中
    W25QXX_CS_LOW();

    // 3.发送指令
    w25qxx_send_byte(self, 0x20);

    // 4.发送地址   数据是高位先出（MSB）
    w25qxx_send_byte(self, (address & 0xFF0000) >> 16);
    w25qxx_send_byte(self, (address & 0xFF00) >> 8);
    w25qxx_send_byte(self, address & 0xFF);

    // 5.把CS片选引脚拉高  表示不通信
    W25QXX_CS_HIGH();

    // 6.等待擦除完成  BUSY位 = 1 表示正在工作  BUSY位=0 表示工作结束
    while (w25qxx_read_status_register(self) & 0x01)
        ;

    // 7.关闭写使能
    w25qxx4_write_disable(self);
}

// 读取数据
void w25qxx_read_data(w25qxx_t* self, u32 address, u8* buf, usize len)
{
    // 1.把CS片选引脚拉低  表示选中
    W25QXX_CS_LOW();

    // 2.发送指令
    w25qxx_send_byte(self, 0x03);

    // 3.发送地址
    w25qxx_send_byte(self, (address & 0xFF0000) >> 16);
    w25qxx_send_byte(self, (address & 0xFF00) >> 8);
    w25qxx_send_byte(self, address & 0xFF);

    // 4.接收数据
    while (len--) {
        *buf++ = w25qxx_send_byte(self, 0xFF);
    }

    // 5.把CS片选引脚拉高  表示不通信
    W25QXX_CS_HIGH();
}

// 扇区写入数据，一次最多写入256字节
void w25qxx_page_program(w25qxx_t* self, u32 address, u8* buf, usize len)
{
    // 1.开启写使能
    w25qxx_write_enable(self);
    // systick_delay_ms(1);

    // 2.把CS片选引脚拉低  表示选中
    W25QXX_CS_LOW();

    // 3.发送指令
    w25qxx_send_byte(self, 0x02);

    // 4.发送地址   数据是高位先出（MSB）
    w25qxx_send_byte(self, (address & 0xFF0000) >> 16);
    w25qxx_send_byte(self, (address & 0xFF00) >> 8);
    w25qxx_send_byte(self, address & 0xFF);

    // 5.发送数据
    while (len--) {
        w25qxx_send_byte(self, *buf++);
    }

    // 6.把CS片选引脚拉高  表示不通信
    W25QXX_CS_HIGH();

    // 7.等待写入完成  BUSY位 = 1 表示忙碌  BUSY位=0 表示空闲
    while (w25qxx_read_status_register(self) & 0x01)
        ;

    // 8.关闭写使能
    w25qxx4_write_disable(self);
}

#if 0
u16 w25qxx_id = 0;
void W25QXX_PowerDown(void);
void W25QXX_WAKEUP(void);
void W25QXX_Erase_Chip(void);
void W25Q64_erase_sector(uint32_t addr);


void W25Qxx_GPIO_Init(void)
{

  W25Q64_CS_1;
  //SPI_CLK0;

//	W25QXX_Erase_Chip();
//	W25Q64_erase_sector(0x00);

  W25QXX_PowerDown();
  Delay_Ms(10);
  W25QXX_WAKEUP();
  Delay_Ms(10);


  w25qxx_id = W25Q64_readID();	      //读取FLASH ID.
}

u16 spi_retry_cnt = 0;
u8 spi_read_write_byte(u8 Byte)
{
  spi_retry_cnt = 0;
  DL_SPI_transmitData8(SPI_1_INST, Byte);  //发送1字节命令

  while(DL_SPI_isBusy(SPI_1_INST))        //等待发送完成
  {
    spi_retry_cnt++;

    if(spi_retry_cnt > 200)	return 0;//超时
  }

  return DL_SPI_receiveData8(SPI_1_INST);  //清空接收FIFO中的数据
}






 /// 函 数 名 称：W25Q64_readID
 /// 函 数 说 明：读取W25Q64的厂商ID和设备ID
 /// 函 数 形 参：无
 /// 函 数 返 回：设备正常返回EF16
 /// 作       者：LCKFB
 /// 备       注：无
u16 W25Q64_readID(void)
{
  u16  temp = 0;
  W25Q64_CS_0;

  spi_read_write_byte(0x90);//发送读取ID命令
  spi_read_write_byte(0x00);
  spi_read_write_byte(0x00);
  spi_read_write_byte(0x00);
  //接收数据
  temp |= spi_read_write_byte(0xFF) << 8;
  temp |= spi_read_write_byte(0xFF);

  W25Q64_CS_1;
  return temp;
}



 /// 函 数 名 称：W25Q64_wait_busy
 /// 函 数 功 能：判断W25Q64是否忙
 /// 传 入 参 数：无
 /// 函 数 返 回：无
 /// 作       者：LCKFB
 /// 备       注：无
void W25Q64_wait_busy(void)
{
  unsigned char byte = 0;

  do
  {
    W25Q64_CS_0;
    spi_read_write_byte(0x05);
    byte = spi_read_write_byte(0Xff);
    W25Q64_CS_1;
  }
  while( ( byte & 0x01 ) == 1 );
}


//进入掉电模式
void W25QXX_PowerDown(void)
{
  W25Q64_CS_0;                            //使能器件
  spi_read_write_byte(W25X_PowerDown);     //发送掉电命令
  W25Q64_CS_1;                            //取消片选
  delay_us(3);                            //等待TPD
}

//唤醒
void W25QXX_WAKEUP(void)
{
  W25Q64_CS_0;                                //使能器件
  spi_read_write_byte(W25X_ReleasePowerDown);  //send W25X_PowerDown command 0xAB
  W25Q64_CS_1;                                //取消片选
  delay_us(3);                                //等待TRES1
}



 /// 函 数 名 称：W25Q64_write_enable
 /// 函 数 功 能：发送写使能
 /// 传 入 参 数：无
 /// 函 数 返 回：无
 /// 作       者：LCKFB
 /// 备       注：无
void W25Q64_write_enable(void)
{
  W25Q64_CS_0;
  spi_read_write_byte(0x06);
  W25Q64_CS_1;
}

 /// 函 数 名 称：W25Q64_erase_sector
 /// 函 数 功 能：擦除一个扇区
 /// 传 入 参 数：addr=擦除的扇区号
 /// 函 数 返 回：无
 /// 作       者：LCKFB
 /// 备       注：addr=擦除的扇区号，范围=0~15
void W25Q64_erase_sector(uint32_t addr)
{
  addr *= 4096;
  W25Q64_write_enable();  //写使能
  W25Q64_wait_busy();     //判断忙
  W25Q64_CS_0;
  spi_read_write_byte(0x20);
  spi_read_write_byte((u8)((addr) >> 16));
  spi_read_write_byte((u8)((addr) >> 8));
  spi_read_write_byte((u8)addr);
  W25Q64_CS_1;
  //等待擦除完成
  W25Q64_wait_busy();
}


 /// 函 数 名 称：W25Q64_write
 /// 函 数 功 能：写数据到W25Q64进行保存
 /// 传 入 参 数：buffer=写入的数据内容	addr=写入地址	numbyte=写入数据的长度
 /// 函 数 返 回：无
 /// 作       者：LCKFB
 /// 备       注：无
void W25Q64_write(u8* buffer, uint32_t addr, u16 numbyte)
{
  //0x02e21
  unsigned int i = 0;
  W25Q64_erase_sector(addr / 4096); //擦除扇区数据
  W25Q64_write_enable();//写使能
  W25Q64_wait_busy(); //忙检测
  //写入数据
  W25Q64_CS_0;
  spi_read_write_byte(0x02);
  spi_read_write_byte((u8)((addr) >> 16));
  spi_read_write_byte((u8)((addr) >> 8));
  spi_read_write_byte((u8)addr);

  for(i = 0; i < numbyte; i++)
  {
    spi_read_write_byte(buffer[i]);
  }

  W25Q64_CS_1;
  W25Q64_wait_busy(); //忙检测
}

 /// 函 数 名 称：W25Q64_read
 /// 函 数 功 能：读取W25Q64的数据
 /// 传 入 参 数：buffer=读出数据的保存地址  read_addr=读取地址		read_length=读去长度
 /// 函 数 返 回：无
 /// 作       者：LCKFB
 /// 备       注：无
void W25Q64_read(u8* buffer, uint32_t read_addr, u16 read_length)
{
  u16 i;
  W25Q64_CS_0;
  spi_read_write_byte(0x03);
  spi_read_write_byte((u8)((read_addr) >> 16));
  spi_read_write_byte((u8)((read_addr) >> 8));
  spi_read_write_byte((u8)read_addr);

  for(i = 0; i < read_length; i++)
  {
    buffer[i] = spi_read_write_byte(0XFF);
  }

  W25Q64_CS_1;
}



//namelesstech
void W25QXX_Erase_Chip(void)
{
  W25Q64_write_enable();                  //SET WEL
  W25Q64_wait_busy();
  W25Q64_CS_0; //使能器件
  spi_read_write_byte(0xC7);     //发送片擦除命令
  W25Q64_CS_1; //取消片选
  W25Q64_wait_busy();   				          //等待芯片擦除结束
}


void W25Q64_write_page(u8* pBuffer, uint32_t WriteAddr, u16 NumByteToWrite)
{
  u16 i;
  W25Q64_write_enable();                  //SET WEL
  W25Q64_CS_0; //使能器件
  spi_read_write_byte(0x02);   //发送写页命令
  spi_read_write_byte((u8)((WriteAddr) >> 16)); //发送24bit地址
  spi_read_write_byte((u8)((WriteAddr) >> 8));
  spi_read_write_byte((u8)WriteAddr);

  for(i = 0; i < NumByteToWrite; i++)	spi_read_write_byte(pBuffer[i]); //循环写数

  W25Q64_CS_1; //取消片选
  W25Q64_wait_busy();					   					//等待写入结束
}


//无检验写SPI FLASH
//必须确保所写的地址范围内的数据全部为0XFF,否则在非0XFF处写入的数据将失败!
//具有自动换页功能
//在指定地址开始写入指定长度的数据,但是要确保地址不越界!
//pBuffer:数据存储区
//WriteAddr:开始写入的地址(24bit)
//NumByteToWrite:要写入的字节数(最大65535)
//CHECK OK
void W25QXX_Write_NoCheck(u8* pBuffer, uint32_t WriteAddr, u16 NumByteToWrite)
{
  u16 pageremain;
  pageremain = 256 - WriteAddr % 256; //单页剩余的字节数

  if(NumByteToWrite <= pageremain)pageremain = NumByteToWrite; //不大于256个字节

  while(1)
  {
    W25Q64_write_page(pBuffer, WriteAddr, pageremain);

    if(NumByteToWrite == pageremain)	break; //写入结束了
    else //NumByteToWrite>pageremain
    {
      pBuffer += pageremain;
      WriteAddr += pageremain;

      NumByteToWrite -= pageremain;			 //减去已经写入了的字节数

      if(NumByteToWrite > 256)pageremain = 256; //一次可以写入256个字节
      else pageremain = NumByteToWrite; 	 //不够256个字节了
    }
  }
}

//写SPI FLASH
//在指定地址开始写入指定长度的数据
//该函数带擦除操作!
//pBuffer:数据存储区
//WriteAddr:开始写入的地址(24bit)
//NumByteToWrite:要写入的字节数(最大65535)
u8 W25QXX_BUFFER[4096];
void W25QXX_Write(u8* pBuffer, uint32_t WriteAddr, u16 NumByteToWrite)
{
  uint32_t secpos;
  u16 secoff;
  u16 secremain;
  u16 i;
  u8 * W25QXX_BUF;
  W25QXX_BUF = W25QXX_BUFFER;
  secpos = WriteAddr / 4096; //扇区地址
  secoff = WriteAddr % 4096; //在扇区内的偏移
  secremain = 4096 - secoff; //扇区剩余空间大小

  //printf("ad:%X,nb:%X\r\n",WriteAddr,NumByteToWrite);//测试用
  if(NumByteToWrite <= secremain)secremain = NumByteToWrite; //不大于4096个字节

  while(1)
  {
    W25Q64_read(W25QXX_BUF, secpos * 4096, 4096); //读出整个扇区的内容

    for(i = 0; i < secremain; i++) //校验数据
    {
      if(W25QXX_BUF[secoff + i] != 0XFF)break; //需要擦除
    }

    if(i < secremain) //需要擦除
    {
      W25Q64_erase_sector(secpos); //擦除这个扇区

      for(i = 0; i < secremain; i++)	 //复制
      {
        W25QXX_BUF[i + secoff] = pBuffer[i];
      }

      W25QXX_Write_NoCheck(W25QXX_BUF, secpos * 4096, 4096); //写入整个扇区

    }
    else W25QXX_Write_NoCheck(pBuffer, WriteAddr, secremain); //写已经擦除了的,直接写入扇区剩余区间.

    if(NumByteToWrite == secremain)break; //写入结束了
    else//写入未结束
    {
      secpos++;//扇区地址增1
      secoff = 0; //偏移位置为0

      pBuffer += secremain; //指针偏移
      WriteAddr += secremain; //写地址偏移
      NumByteToWrite -= secremain;				//字节数递减

      if(NumByteToWrite > 4096)secremain = 4096;	//下一个扇区还是写不完
      else secremain = NumByteToWrite;					//下一个扇区可以写完了
    }
  };
}


void W25QXX_Write_Data(float *data, uint32_t addr)
{
  u8 tempwr[4];
  tempwr[0] = (*(uint32_t *)(data)) & 0xff;
  tempwr[1] = (*(uint32_t *)(data)) >> 8;
  tempwr[2] = (*(uint32_t *)(data)) >> 16;
  tempwr[3] = (*(uint32_t *)(data)) >> 24;
  //W25QXX_Write_Page(tempwr,addr,4);
  W25QXX_Write(tempwr, addr, 4);
}


typedef union
{
  unsigned char Bdata[4];
  float Fdata;
  uint32_t Idata;
} Float_2_Byte;


void W25QXX_Read_Float(float *data, uint32_t addr)
{
  Float_2_Byte temp;
  W25Q64_read(temp.Bdata, addr, 4);
  *data = temp.Fdata;
}

void W25QXX_Read_f(float *pui32Data, uint32_t ui32Address, uint32_t ui32Count)
{
  Float_2_Byte temp;

  for(u16 i = 0; i < ui32Count; i++)
  {
    W25Q64_read(temp.Bdata, ui32Address + 4 * i, 4);
    *pui32Data = temp.Fdata;
    pui32Data++;
  }
}


void W25QXX_Read_fs(float *Data, uint32_t ui32Address, uint32_t ui32Count)
{
  for(u16 i = 0; i < ui32Count; i++)
  {
    Float_2_Byte temp;
    W25Q64_read(&temp.Bdata[0], ui32Address + 4 * i + 0, 1);
    W25Q64_read(&temp.Bdata[1], ui32Address + 4 * i + 1, 1);
    W25Q64_read(&temp.Bdata[2], ui32Address + 4 * i + 2, 1);
    W25Q64_read(&temp.Bdata[3], ui32Address + 4 * i + 3, 1);
    *Data = temp.Fdata;
    Data++;
  }
}


void W25QXX_Write_f(float *data, uint32_t ui32Address, uint32_t ui32Count)
{
  Float_2_Byte temp;

  for(u16 i = 0; i < ui32Count; i++)
  {
    temp.Fdata = *data;
    W25QXX_Write(temp.Bdata, ui32Address + 4 * i, 4);
    data++;
  }
}

void Resume_Factory_Setting(void)
{
  W25QXX_Erase_Chip();
  W25Q64_erase_sector(0x00);
}

#endif
