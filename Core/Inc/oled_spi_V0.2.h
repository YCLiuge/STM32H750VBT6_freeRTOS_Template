#ifndef __OLED_SPI_V0_2_H
#define __OLED_SPI_V0_2_H

#include "stm32h7xx_hal.h"
#include "main.h"
#include "spi.h"

#define OLED_CMD  0
#define OLED_DATA 1
#define OLED_MODE 0

#define u8  unsigned char
#define u32 unsigned int

/*-----------------OLED 接口选择----------------
 * OLED_IFACE_HAL_SPI  (0): 硬件 SPI4 — PE12(SCK), PE14(MOSI), PE11(NSS)
 * OLED_IFACE_SOFT_SPI (1): 软件模拟 SPI  — 需自行定义 SCLK/SDIN/CS 引脚宏
 * OLED_IFACE_8080     (2): 8位并行 8080  — 需 WR/RD/CS/D0~D7 总线
 */
#define OLED_IFACE_HAL_SPI  0
#define OLED_IFACE_SOFT_SPI 1
#define OLED_IFACE_8080     2

#ifndef OLED_IFACE
#define OLED_IFACE  OLED_IFACE_HAL_SPI
#endif

/*-----------------OLED 端口定义----------------
 * HAL_SPI 模式: 硬件 SPI4 — PE12(SCK), PE14(MOSI), PE11(NSS)
 * GPIO: PE10(DC), PE9(RES)
 * 3-Wire SPI (TX only, no MISO)
 */
#define OLED_DC_Clr()  HAL_GPIO_WritePin(OLED_DC_GPIO_Port, OLED_DC_Pin, GPIO_PIN_RESET)
#define OLED_DC_Set()  HAL_GPIO_WritePin(OLED_DC_GPIO_Port, OLED_DC_Pin, GPIO_PIN_SET)

#define OLED_RST_Clr() HAL_GPIO_WritePin(OLED_RES_GPIO_Port, OLED_RES_Pin, GPIO_PIN_RESET)
#define OLED_RST_Set() HAL_GPIO_WritePin(OLED_RES_GPIO_Port, OLED_RES_Pin, GPIO_PIN_SET)

/*
 * 软件 SPI 模式引脚定义（选择 OLED_IFACE_SOFT_SPI 时取消注释并配置）
 *
 * #define OLED_SCLK_Clr()  HAL_GPIO_WritePin(OLED_SCLK_GPIO_Port, OLED_SCLK_Pin, GPIO_PIN_RESET)
 * #define OLED_SCLK_Set()  HAL_GPIO_WritePin(OLED_SCLK_GPIO_Port, OLED_SCLK_Pin, GPIO_PIN_SET)
 * #define OLED_SDIN_Clr()  HAL_GPIO_WritePin(OLED_SDIN_GPIO_Port, OLED_SDIN_Pin, GPIO_PIN_RESET)
 * #define OLED_SDIN_Set()  HAL_GPIO_WritePin(OLED_SDIN_GPIO_Port, OLED_SDIN_Pin, GPIO_PIN_SET)
 * #define OLED_CS_Clr()    HAL_GPIO_WritePin(OLED_CS_GPIO_Port, OLED_CS_Pin, GPIO_PIN_RESET)
 * #define OLED_CS_Set()    HAL_GPIO_WritePin(OLED_CS_GPIO_Port, OLED_CS_Pin, GPIO_PIN_SET)
 *
 * 并行 8080 模式引脚定义（选择 OLED_IFACE_8080 时取消注释并配置）
 *
 * #define DATAOUT(x)   GPIOx->ODR = (GPIOx->ODR & 0xFF00) | (x)  // D0~D7 并口输出
 * #define OLED_WR_Clr() HAL_GPIO_WritePin(OLED_WR_GPIO_Port,  OLED_WR_Pin,  GPIO_PIN_RESET)
 * #define OLED_WR_Set() HAL_GPIO_WritePin(OLED_WR_GPIO_Port,  OLED_WR_Pin,  GPIO_PIN_SET)
 * #define OLED_CS_Clr() HAL_GPIO_WritePin(OLED_CS_GPIO_Port,  OLED_CS_Pin,  GPIO_PIN_RESET)
 * #define OLED_CS_Set() HAL_GPIO_WritePin(OLED_CS_GPIO_Port,  OLED_CS_Pin,  GPIO_PIN_SET)
 */

/* OLED 模式设置
 * 0: 4线串行模式
 * 1: 并行8080模式
 */
#define CPU_Frq    80000   /* Unit: kHz */
#define SIZE       12
#define XLevelL    0x02
#define XLevelH    0x10
#define Max_Column 128
#define Max_Row    64
#define Brightness 0xFF
#define X_WIDTH    128
#define Y_WIDTH    64
#if (SIZE == 16)
  #define YOFFSET 1
#else
  #define YOFFSET 0
#endif

/* OLED 控制用函数 */
void OLED_WR_Byte(u8 dat, u8 cmd);
void OLED_Display_On(void);
void OLED_Display_Off(void);
void OLED_Init(void);
void OLED_Clear(void);
void OLED_DrawPoint(u8 x, u8 y, u8 t);
void OLED_Fill(u8 x1, u8 y1, u8 x2, u8 y2, u8 dot);
void OLED_ShowChar(u8 x, u8 y, char chr);
void OLED_ShowNum(u8 x, u8 y, u32 num, u8 len, u8 size2);
void OLED_ShowSignedNum(u8 x, u8 y, int32_t num, u8 len, u8 size2);
void OLED_ShowString(u8 x, u8 y, char *p);
void OLED_Set_Pos(unsigned char x, unsigned char y);
void OLED_ShowCHinese(u8 x, u8 y, u8 no);
void OLED_DrawBMP(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1, const unsigned char BMP[]);
void OLED_ColorTurn(u8 i);
void OLED_DisplayTurn(u8 i);
void OLED_Refresh(void);
void OLED_DrawLine(u8 x1, u8 y1, u8 x2, u8 y2, u8 mode);
void OLED_DrawCircle(u8 x, u8 y, u8 r);
void OLED_ShowFloat(u8 x, u8 y, float num, u8 int_digits, u8 frac_digits);

#endif
