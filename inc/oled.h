#ifndef __OLED_H
#define __OLED_H

/* ============================================================================
 * oled.h —— OLED 驱动对外接口 (与单片机无关, 换 MCU 不用改本文件)
 * ----------------------------------------------------------------------------
 * 【怎么用】主程序里只需要:  #include "oled.h"   然后调用下面的函数。
 * 【屏幕坐标】128 列 x 64 行。x:0~127(从左到右), y:0~63(从上到下)。
 *            内部按“页”管理: 64 行 = 8 页, 每页 8 行高。
 * 【显存机制】所有绘图/文字先写进内存缓冲区(OLED_GRAM), 再调用 OLED_Refresh()
 *            一次性刷到屏上。好处: 画面无闪烁、可叠加。
 * 【可改参数】见 oled_port.h (引脚、地址、速度)。本文件一般不用改。
 * ==========================================================================*/

#include <stdint.h>
#include "oled_port.h"

#define OLED_W  128   /* 屏宽(列) */
#define OLED_H  64    /* 屏高(行) */

/* ---- 初始化 / 全局控制 ---- */
void OLED_Init(void);                 /* 初始化(含开启电荷泵), 上电必须先调用 */
void OLED_Clear(void);                /* 清屏(缓冲区清 0) */
void OLED_Refresh(void);              /* 把缓冲区刷到屏幕(改完内容必须调用) */
void OLED_Display_On(void);           /* 开显示 */
void OLED_Display_Off(void);          /* 关显示(省电) */
void OLED_SetBrightness(uint8_t val); /* 亮度/对比度 0~255 */

/* ---- 像素 / 图形 (坐标越界会被自动忽略) ---- */
void OLED_DrawPoint(uint8_t x, uint8_t y, uint8_t on);          /* on:1亮 0灭 */
void OLED_DrawLine(uint8_t x1,uint8_t y1,uint8_t x2,uint8_t y2);/* 画直线 */
void OLED_DrawRect(uint8_t x,uint8_t y,uint8_t w,uint8_t h);    /* 画空心矩形 */

/* ---- 文字 / 数字 ---- */
/* size: 16=8x16字体, 8=6x8字体 */
void OLED_ShowChar(uint8_t x,uint8_t y,char ch,uint8_t size);
void OLED_ShowString(uint8_t x,uint8_t y,const char *str,uint8_t size);
void OLED_ShowNum(uint8_t x,uint8_t y,uint32_t num,uint8_t len,uint8_t size);

/* ---- 位图 ---- */
/* 显示单色位图, bmp 为“列行式、逆向”取模数据(与字库同格式) */
void OLED_ShowBMP(uint8_t x,uint8_t y,uint8_t w,uint8_t h,const uint8_t *bmp);

extern uint8_t OLED_GRAM[8][128];   /* 显存缓冲区: [页][列], 需要时可直接操作 */

#endif /* __OLED_H */
