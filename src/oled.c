/* ============================================================================
 * oled.c —— OLED 协议 & 绘图逻辑 (与单片机无关)
 * ----------------------------------------------------------------------------
 * 本文件只调用 oled_port.h 提供的接口, 不直接操作任何寄存器。
 * 所以换单片机时本文件【原样复用】, 只改 oled_port.c/.h 即可。
 * ==========================================================================*/
#include "oled.h"
#include "oled_font.h"

/* 显存: 8 页 x 128 列, 每字节代表某列 8 个像素(bit0 在该页最上一行) */
uint8_t OLED_GRAM[8][128];

/* 初始化命令序列 (SSD1306/SSD1315 通用) */
static const uint8_t init_cmds[] = {
    0xAE,             /* 关显示 */
    0x20, 0x00,       /* 内存寻址模式: 水平 */
    0xB0,             /* 页起始地址 */
    0xC8,             /* COM 扫描方向(上下翻转用 0xC0) */
    0x00, 0x10,       /* 列起始地址低/高 */
    0x40,             /* 显示起始行 */
    0x81, 0xCF,       /* 对比度 */
    0xA1,             /* 段重映射(左右翻转用 0xA0) */
    0xA6,             /* 正常显示(反显用 0xA7) */
    0xA8, 0x3F,       /* 复用比 1/64 */
    0xD3, 0x00,       /* 显示偏移 */
    0xD5, 0x80,       /* 时钟分频 */
    0xD9, 0xF1,       /* 预充电周期 */
    0xDA, 0x12,       /* COM 引脚配置 */
    0xDB, 0x40,       /* VCOMH */
    0x8D, 0x14,       /* 【关键】开启电荷泵(不开则永远黑屏!) */
    0xAF              /* 开显示 */
};

void OLED_Init(void)
{
    uint32_t i;
    OLED_Port_Init();
    OLED_Port_DelayMs(100);              /* 等 OLED 上电稳定 */
    for (i = 0; i < sizeof(init_cmds); i++)
        OLED_Port_WriteCmd(init_cmds[i]);
    OLED_Clear();
    OLED_Refresh();
}

void OLED_Clear(void)
{
    uint16_t i, j;
    for (i = 0; i < 8; i++)
        for (j = 0; j < 128; j++)
            OLED_GRAM[i][j] = 0x00;
}

void OLED_Refresh(void)
{
    uint8_t page;
    for (page = 0; page < 8; page++) {
        OLED_Port_WriteCmd(0xB0 + page); /* 设置页地址 */
        OLED_Port_WriteCmd(0x00);        /* 列低地址 */
        OLED_Port_WriteCmd(0x10);        /* 列高地址 */
        OLED_Port_WriteDataBuf(OLED_GRAM[page], 128);
    }
}

void OLED_Display_On(void)  { OLED_Port_WriteCmd(0xAF); }
void OLED_Display_Off(void) { OLED_Port_WriteCmd(0xAE); }
void OLED_SetBrightness(uint8_t v) { OLED_Port_WriteCmd(0x81); OLED_Port_WriteCmd(v); }

void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t on)
{
    if (x >= OLED_W || y >= OLED_H) return;
    if (on) OLED_GRAM[y / 8][x] |=  (1 << (y % 8));
    else    OLED_GRAM[y / 8][x] &= ~(1 << (y % 8));
}

/* Bresenham 直线 */
void OLED_DrawLine(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2)
{
    int dx =  (x2>=x1)?(x2-x1):(x1-x2);
    int dy = -((y2>=y1)?(y2-y1):(y1-y2));
    int sx = (x1<x2)?1:-1;
    int sy = (y1<y2)?1:-1;
    int err = dx + dy, e2;
    while (1) {
        OLED_DrawPoint(x1, y1, 1);
        if (x1==x2 && y1==y2) break;
        e2 = 2*err;
        if (e2 >= dy) { err += dy; x1 += sx; }
        if (e2 <= dx) { err += dx; y1 += sy; }
    }
}

void OLED_DrawRect(uint8_t x,uint8_t y,uint8_t w,uint8_t h)
{
    OLED_DrawLine(x, y, x+w-1, y);
    OLED_DrawLine(x, y+h-1, x+w-1, y+h-1);
    OLED_DrawLine(x, y, x, y+h-1);
    OLED_DrawLine(x+w-1, y, x+w-1, y+h-1);
}

/* 显示一个字符。size: 16=8x16, 8=6x8 */
void OLED_ShowChar(uint8_t x, uint8_t y, char ch, uint8_t size)
{
    uint8_t i, col, page;
    if (ch < 32 || ch > 126) ch = 32;   /* 越界字符显示空格 */
    uint8_t idx = (uint8_t)ch - 32;

    if (size == 16) {                    /* 8x16: 上下各一页 */
        for (page = 0; page < 2; page++)
            for (col = 0; col < 8; col++) {
                uint8_t by = OLED_F8x16[idx][page*8 + col];
                for (i = 0; i < 8; i++)
                    OLED_DrawPoint(x+col, y + page*8 + i, (by>>i)&0x01);
            }
    } else {                             /* 6x8 */
        for (col = 0; col < 6; col++) {
            uint8_t by = OLED_F6x8[idx][col];
            for (i = 0; i < 8; i++)
                OLED_DrawPoint(x+col, y + i, (by>>i)&0x01);
        }
    }
}

void OLED_ShowString(uint8_t x, uint8_t y, const char *str, uint8_t size)
{
    uint8_t step = (size == 16) ? 8 : 6;
    while (*str) {
        OLED_ShowChar(x, y, *str++, size);
        x += step;
        if (x > OLED_W - step) { x = 0; y += size; }  /* 自动换行 */
    }
}

/* 显示无符号整数。len=显示位数(前面补 0) */
void OLED_ShowNum(uint8_t x, uint8_t y, uint32_t num, uint8_t len, uint8_t size)
{
    uint8_t step = (size == 16) ? 8 : 6;
    char buf[11];
    int8_t i;
    for (i = len - 1; i >= 0; i--) { buf[i] = '0' + (num % 10); num /= 10; }
    buf[len] = '\0';
    for (i = 0; i < len; i++) { OLED_ShowChar(x, y, buf[i], size); x += step; }
}

/* 显示单色位图(列行式逆向取模, 与字库同格式) */
void OLED_ShowBMP(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bmp)
{
    uint8_t col, page, i;
    uint8_t pages = (h + 7) / 8;
    for (page = 0; page < pages; page++)
        for (col = 0; col < w; col++) {
            uint8_t by = bmp[page*w + col];
            for (i = 0; i < 8; i++)
                if (y + page*8 + i < OLED_H)
                    OLED_DrawPoint(x+col, y + page*8 + i, (by>>i)&0x01);
        }
}
