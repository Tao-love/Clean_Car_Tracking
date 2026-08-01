/* ============================================================================
 * oled_port.c —— 硬件移植层实现 (MSPM0G3507 软件模拟 I2C)
 * ----------------------------------------------------------------------------
 * 用普通 GPIO 位翻转 (bit-bang) 模拟标准 I2C 时序。
 * 采用开漏方式: 输出低=拉低; 释放=靠上拉电阻(模块自带4.7K)回到高。
 * SDA 释放后可读回, 用于检测从机 ACK 应答 —— 这是没有逻辑分析仪时的验证依据。
 * ==========================================================================*/
#include "oled_port.h"

/* 调试观测变量: 在 CCS Expressions 窗口输入这两个名字即可实时查看 */
volatile uint8_t  g_oled_last_ack = 0;
volatile uint32_t g_oled_ack_fail = 0;

/* ---- 底层引脚操作 (改引脚不用动这里, 宏已在 .h 里配置好) ---- */
#define SCL_HIGH()  DL_GPIO_setPins(OLED_PORT, OLED_SCL_PIN)
#define SCL_LOW()   DL_GPIO_clearPins(OLED_PORT, OLED_SCL_PIN)
/* SDA 用真开漏: 拉低=使能输出(引脚已预置为0); 释放=关输出让上拉拉高 */
#define SDA_LOW()   DL_GPIO_enableOutput(OLED_PORT, OLED_SDA_PIN)
#define SDA_RELEASE() DL_GPIO_disableOutput(OLED_PORT, OLED_SDA_PIN)
#define SDA_READ()  ((DL_GPIO_readPins(OLED_PORT, OLED_SDA_PIN) & OLED_SDA_PIN) ? 1 : 0)

/* I2C 时序延时 */
static void i2c_delay(void)
{
    volatile uint32_t i = OLED_I2C_DELAY;
    while (i--) { __asm volatile("nop"); }
}

/* 引脚初始化 */
void OLED_Port_Init(void)
{
    /* SCL: 普通推挽输出, 初始高 */
    DL_GPIO_initDigitalOutput(OLED_SCL_IOMUX);
    DL_GPIO_setPins(OLED_PORT, OLED_SCL_PIN);
    DL_GPIO_enableOutput(OLED_PORT, OLED_SCL_PIN);

    /* SDA: 预先把输出寄存器写 0, 并使能输入缓冲(才能读回 ACK)。
     * 之后靠 enableOutput/disableOutput 切换 “拉低 / 释放”, 实现开漏。*/
    DL_GPIO_initDigitalOutput(OLED_SDA_IOMUX);
    DL_GPIO_clearPins(OLED_PORT, OLED_SDA_PIN);   /* 输出寄存器=0 */
    DL_GPIO_disableOutput(OLED_PORT, OLED_SDA_PIN);/* 先释放(高) */
}

/* I2C 起始: SCL高时 SDA 由高变低 */
static void i2c_start(void)
{
    SDA_RELEASE(); SCL_HIGH(); i2c_delay();
    SDA_LOW();     i2c_delay();
    SCL_LOW();     i2c_delay();
}

/* I2C 停止: SCL高时 SDA 由低变高 */
static void i2c_stop(void)
{
    SDA_LOW();     SCL_HIGH(); i2c_delay();
    SDA_RELEASE(); i2c_delay();
}

/* 发送一个字节, 并读取从机 ACK。返回 0=收到ACK, 1=NACK */
static uint8_t i2c_write_byte(uint8_t b)
{
    uint8_t i, ack;
    for (i = 0; i < 8; i++) {
        if (b & 0x80) SDA_RELEASE(); else SDA_LOW();  /* 先发高位 */
        i2c_delay();
        SCL_HIGH(); i2c_delay();
        SCL_LOW();  i2c_delay();
        b <<= 1;
    }
    /* 第 9 个时钟读 ACK: 释放 SDA, 让从机拉低 */
    SDA_RELEASE(); i2c_delay();
    SCL_HIGH();    i2c_delay();
    ack = SDA_READ();          /* 0=从机应答成功 */
    SCL_LOW();     i2c_delay();

    g_oled_last_ack = ack;
    if (ack) g_oled_ack_fail++;
    return ack;
}

/* 写一条命令: [起始][地址][控制字0x00=命令][命令][停止] */
void OLED_Port_WriteCmd(uint8_t cmd)
{
    i2c_start();
    i2c_write_byte(OLED_I2C_ADDR);
    i2c_write_byte(0x00);
    i2c_write_byte(cmd);
    i2c_stop();
}

/* 写一个数据字节: 控制字 0x40=数据 */
void OLED_Port_WriteData(uint8_t dat)
{
    i2c_start();
    i2c_write_byte(OLED_I2C_ADDR);
    i2c_write_byte(0x40);
    i2c_write_byte(dat);
    i2c_stop();
}

/* 连续写多个数据字节(整屏刷新用, 一次事务发完, 效率高) */
void OLED_Port_WriteDataBuf(uint8_t *buf, uint16_t len)
{
    uint16_t i;
    i2c_start();
    i2c_write_byte(OLED_I2C_ADDR);
    i2c_write_byte(0x40);
    for (i = 0; i < len; i++) i2c_write_byte(buf[i]);
    i2c_stop();
}

/* 毫秒延时(粗略, 按默认主频估算; 不要求精确, 仅用于上电等待) */
void OLED_Port_DelayMs(uint32_t ms)
{
    volatile uint32_t n;
    while (ms--) { n = 8000; while (n--) { __asm volatile("nop"); } }
}
