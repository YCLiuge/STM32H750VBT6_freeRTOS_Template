#include "oled_spi_V0.2.h"
#include "oledfont.h"
#include <string.h>

/* 向 SSD1306 写入一个字节
 * dat: 要写入的数据/命令
 * cmd: 0=命令; 1=数据;
 */
u8 OLED_GRAM[130][8];

/* ================================================================
 * OLED_WR_Byte — 接口选择 (在 oled_spi_V0.2.h 中定义 OLED_IFACE)
 * ================================================================ */
#if (OLED_IFACE == OLED_IFACE_HAL_SPI)
/*
 * 硬件 SPI (HAL) — 当前模式
 */
void OLED_WR_Byte(u8 dat, u8 cmd)
{
    if (cmd)
        OLED_DC_Set();
    else
        OLED_DC_Clr();

    while (HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY) { }
    HAL_SPI_Transmit(&hspi4, &dat, 1, HAL_MAX_DELAY);
    while (HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY) { }

    OLED_DC_Set();
}

#elif (OLED_IFACE == OLED_IFACE_SOFT_SPI)
/*
 * 软件模拟 SPI — MCU 无硬件 SPI 或需要任意引脚时使用
 * 需要在 oled_spi_V0.2.h 中取消注释并配置 SCLK/SDIN/CS 引脚宏
 */
void OLED_WR_Byte(u8 dat, u8 cmd)
{
    u8 i;
    if (cmd)  OLED_DC_Set();
    else      OLED_DC_Clr();
    OLED_CS_Clr();
    for (i = 0; i < 8; i++)
    {
        OLED_SCLK_Clr();
        if (dat & 0x80)  OLED_SDIN_Set();
        else             OLED_SDIN_Clr();
        OLED_SCLK_Set();
        dat <<= 1;
    }
    OLED_CS_Set();
    OLED_DC_Set();
}

#elif (OLED_IFACE == OLED_IFACE_8080)
/*
 * 8 位并行 8080 总线 — 高速刷新场景
 * 需要在 oled_spi_V0.2.h 中定义 DATAOUT / WR / CS 引脚宏
 */
void OLED_WR_Byte(u8 dat, u8 cmd)
{
    DATAOUT(dat);
    if (cmd)  OLED_DC_Set();
    else      OLED_DC_Clr();
    OLED_CS_Clr();
    OLED_WR_Clr();
    OLED_WR_Set();
    OLED_CS_Set();
    OLED_DC_Set();
}

#endif /* OLED_IFACE */

/* ================================================================ */
void OLED_Set_Pos(unsigned char x, unsigned char y)
{
    OLED_WR_Byte(0xAF + y + YOFFSET, OLED_CMD);  /* page address: 0xB0~0xB7 */
    OLED_WR_Byte(((x & 0xf0) >> 4) | 0x10, OLED_CMD);  /* column high nibble */
    OLED_WR_Byte((x & 0x0f) | 0x01, OLED_CMD);          /* column low nibble */
}

/* 开启 OLED 显示 */
void OLED_Display_On(void)
{
    OLED_WR_Byte(0x8D, OLED_CMD);  /* SET DCDC */
    OLED_WR_Byte(0x14, OLED_CMD);  /* DCDC ON */
    OLED_WR_Byte(0xAF, OLED_CMD);  /* DISPLAY ON */
}

/* 关闭 OLED 显示 */
void OLED_Display_Off(void)
{
    OLED_WR_Byte(0x8D, OLED_CMD);  /* SET DCDC */
    OLED_WR_Byte(0x10, OLED_CMD);  /* DCDC OFF */
    OLED_WR_Byte(0xAE, OLED_CMD);  /* DISPLAY OFF */
}

/* 清屏函数 — 清完后整个屏幕是黑色的 */
void OLED_Clear(void)
{
    u8 i, n;
    for (i = 0; i < 8; i++)
    {
        OLED_WR_Byte(0xB0 + i, OLED_CMD);  /* 设置页地址 (0~7) */
        OLED_WR_Byte(0x02, OLED_CMD);      /* 设置列低地址 */
        OLED_WR_Byte(0x10, OLED_CMD);      /* 设置列高地址 */
        for (n = 0; n < 128; n++)
            OLED_WR_Byte(0, OLED_DATA);    /* 写入 0（黑） */
    }
    memset(OLED_GRAM, 0, sizeof(OLED_GRAM));
}

/* 在指定位置显示一个字符
 * x: 0~127
 * y: 页地址 (0~7)
 */
void OLED_ShowChar(u8 x, u8 y, char chr)
{
    unsigned char c = 0, i = 0;
    c = chr - ' ';  /* 偏移后的索引 */
    if (x > Max_Column - 1)
    {
        x = 0;
        y = y + 2;
    }
    if (SIZE == 16)
    {
        OLED_Set_Pos(x, y);
        for (i = 0; i < 8; i++)
            OLED_WR_Byte(F8X16[c * 16 + i], OLED_DATA);
        OLED_Set_Pos(x, y + 1);
        for (i = 0; i < 8; i++)
            OLED_WR_Byte(F8X16[c * 16 + i + 8], OLED_DATA);
    }
    else
    {
        OLED_Set_Pos(x, y + 1);
        for (i = 0; i < 6; i++)
            OLED_WR_Byte(F6x8[c][i], OLED_DATA);
    }
}

/* m^n 函数 */
u32 oled_pow(u8 m, u8 n)
{
    u32 result = 1;
    while (n--)
        result *= m;
    return result;
}

/* 显示数字
 * x,y: 起点坐标
 * len: 数字的位数
 * size2: 字体大小
 * num: 数字 (0~4294967295)
 */
void OLED_ShowNum(u8 x, u8 y, u32 num, u8 len, u8 size2)
{
    u8 t, temp;
    u8 enshow = 0;
    for (t = 0; t < len; t++)
    {
        temp = (num / oled_pow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                OLED_ShowChar(x + (size2 / 2) * t, y, ' ');
                continue;
            }
            else
                enshow = 1;
        }
        OLED_ShowChar(x + (size2 / 2) * t, y, temp + '0');
    }
}

/* 可显示负数的数字显示 */
void OLED_ShowSignedNum(u8 x, u8 y, int32_t num, u8 len, u8 size2)
{
    u8 t, temp;
    u8 enshow = 0;
    u8 isNegative = 0;

    if (num < 0)
    {
        isNegative = 1;
        num = -num;
    }

    if (isNegative)
    {
        OLED_ShowChar(x, y, '-');
        x += size2 / 2;  /* 更新起点坐标 */
        len--;            /* 减少一位用于显示负号 */
    }

    for (t = 0; t < len; t++)
    {
        temp = (num / oled_pow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                OLED_ShowChar(x + (size2 / 2) * t, y, ' ');
                continue;
            }
            else
                enshow = 1;
        }
        OLED_ShowChar(x + (size2 / 2) * t, y, temp + '0');
    }
}

/* 显示一个字符串 */
void OLED_ShowString(u8 x, u8 y, char *chr)
{
    unsigned char j = 0;
    while (chr[j] != '\0')
    {
        OLED_ShowChar(x, y, chr[j]);
        x += 8;
        if (x > 120)
        {
            x = 0;
            y += 2;
        }
        j++;
    }
}

/* 显示汉字 (16×16 点阵) */
void OLED_ShowCHinese(u8 x, u8 y, u8 no)
{
    u8 t, adder = 0;
    OLED_Set_Pos(x, y);
    for (t = 0; t < 16; t++)
    {
        OLED_WR_Byte(Hzk[2 * no][t], OLED_DATA);
        adder += 1;
    }
    OLED_Set_Pos(x, y + 1);
    for (t = 0; t < 16; t++)
    {
        OLED_WR_Byte(Hzk[2 * no + 1][t], OLED_DATA);
        adder += 1;
    }
}

/* 显示 BMP 图片 128×64
 * (x0,y0): 起始点坐标, x:0~127, y: 页范围 0~7
 */
void OLED_DrawBMP(unsigned char x0, unsigned char y0, unsigned char x1, unsigned char y1, const unsigned char BMP[])
{
    unsigned int j = 0;
    unsigned char x, y;

    if (y1 % 8 == 0)
        y = y1 / 8;
    else
        y = y1 / 8 + 1;
    for (y = y0; y < y1; y++)
    {
        OLED_Set_Pos(x0, y + (!YOFFSET));
        for (x = x0; x < x1; x++)
        {
            OLED_WR_Byte(BMP[j++], OLED_DATA);
        }
    }
}

/* 反色函数 */
void OLED_ColorTurn(u8 i)
{
    if (i == 0)
        OLED_WR_Byte(0xA6, OLED_CMD);  /* 正常显示 */
    if (i == 1)
        OLED_WR_Byte(0xA7, OLED_CMD);  /* 反色显示 */
}

/* 旋转 180 度 */
void OLED_DisplayTurn(u8 i)
{
    if (i == 0)
    {
        OLED_WR_Byte(0xC8, OLED_CMD);  /* 正常显示 */
        OLED_WR_Byte(0xA1, OLED_CMD);
    }
    if (i == 1)
    {
        OLED_WR_Byte(0xC0, OLED_CMD);  /* 反向显示 */
        OLED_WR_Byte(0xA0, OLED_CMD);
    }
}

/* 更新显存到 OLED */
void OLED_Refresh(void)
{
    u8 i, n;
    for (i = 0; i < 8; i++)
    {
        OLED_WR_Byte(0xB0 + i, OLED_CMD);  /* 设置页起始地址 */
        OLED_WR_Byte(0x00, OLED_CMD);      /* 设置低列起始地址 */
        OLED_WR_Byte(0x10, OLED_CMD);      /* 设置高列起始地址 */

        while (HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY) { }
        uint8_t data[] = {0x78, 0x78, 0x40};
        HAL_SPI_Transmit(&hspi4, data, sizeof(data), HAL_MAX_DELAY);
        while (HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY) { }
        for (n = 0; n < 128; n++)
        {
            HAL_SPI_Transmit(&hspi4, &OLED_GRAM[n][i], 1, HAL_MAX_DELAY);
            while (HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY) { }
        }
    }
}

/* 画点
 * x: 0~127
 * y: 0~63
 * t: 1=填充, 0=清除
 */
void OLED_DrawPoint(u8 x, u8 y, u8 t)
{
    u8 i, m, n;
    i = y / 8;
    m = y % 8;
    n = 1 << m;
    if (t)
        OLED_GRAM[x][i] |= n;
    else
    {
        OLED_GRAM[x][i] = ~OLED_GRAM[x][i];
        OLED_GRAM[x][i] |= n;
        OLED_GRAM[x][i] = ~OLED_GRAM[x][i];
    }
}

/* 画线
 * (x1,y1): 起点坐标
 * (x2,y2): 结束坐标
 */
void OLED_DrawLine(u8 x1, u8 y1, u8 x2, u8 y2, u8 mode)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, uRow, uCol;
    delta_x = x2 - x1;
    delta_y = y2 - y1;
    uRow = x1;
    uCol = y1;
    if (delta_x > 0) incx = 1;
    else if (delta_x == 0) incx = 0;
    else { incx = -1; delta_x = -delta_x; }
    if (delta_y > 0) incy = 1;
    else if (delta_y == 0) incy = 0;
    else { incy = -1; delta_y = -delta_x; }
    if (delta_x > delta_y) distance = delta_x;
    else distance = delta_y;
    for (t = 0; t < distance + 1; t++)
    {
        OLED_DrawPoint(uRow, uCol, mode);
        xerr += delta_x;
        yerr += delta_y;
        if (xerr > distance)
        {
            xerr -= distance;
            uRow += incx;
        }
        if (yerr > distance)
        {
            yerr -= distance;
            uCol += incy;
        }
    }
}

/* 画圆
 * (x,y): 圆心坐标
 * r: 半径
 */
void OLED_DrawCircle(u8 x, u8 y, u8 r)
{
    int a, b, num;
    a = 0;
    b = r;
    while (2 * b * b >= r * r)
    {
        OLED_DrawPoint(x + a, y - b, 1);
        OLED_DrawPoint(x - a, y - b, 1);
        OLED_DrawPoint(x - a, y + b, 1);
        OLED_DrawPoint(x + a, y + b, 1);

        OLED_DrawPoint(x + b, y + a, 1);
        OLED_DrawPoint(x + b, y - a, 1);
        OLED_DrawPoint(x - b, y - a, 1);
        OLED_DrawPoint(x - b, y + a, 1);

        a++;
        num = (a * a + b * b) - r * r;  /* 计算点到圆心的距离 */
        if (num > 0)
        {
            b--;
            a--;
        }
    }
}

/* 初始化 SSD1306 */
void OLED_Init(void)
{
    /* Reconfigure SPI4 for SSD1306: max SCLK ~10 MHz.
     * SPI4 kernel clock = D2PCLK1 = APB1 = 120 MHz.
     * Prescaler 16 → 7.5 MHz (safe for SSD1306). */
    hspi4.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    HAL_SPI_Init(&hspi4);

    /* SSD1306 hardware reset: RST# low pulse >= 3us then release */
    OLED_RST_Set();
    HAL_Delay(10);
    OLED_RST_Clr();
    HAL_Delay(10);
    OLED_RST_Set();
    HAL_Delay(10);

    OLED_WR_Byte(0xAE, OLED_CMD);  /* Display OFF */
    OLED_WR_Byte(0x02, OLED_CMD);  /* Set Low Column Address */
    OLED_WR_Byte(0x10, OLED_CMD);  /* Set High Column Address */
    OLED_WR_Byte(0x40, OLED_CMD);  /* Set Start Line Address (0x00~0x3F) */
    OLED_WR_Byte(0x81, OLED_CMD);  /* Set Contrast */
    OLED_WR_Byte(0xCF, OLED_CMD);  /* Contrast = 207 */
    OLED_WR_Byte(0xA1, OLED_CMD);  /* Set Segment Re-map (A0=left-right reverse, A1=normal) */
    OLED_WR_Byte(0xC8, OLED_CMD);  /* Set COM Scan Direction (C0=up-down reverse, C8=normal) */
    OLED_WR_Byte(0xA6, OLED_CMD);  /* Set Normal Display */
    OLED_WR_Byte(0xA8, OLED_CMD);  /* Set Multiplex Ratio */
    OLED_WR_Byte(0x3F, OLED_CMD);  /* 1/64 duty */
    OLED_WR_Byte(0xD3, OLED_CMD);  /* Set Display Offset */
    OLED_WR_Byte(0x00, OLED_CMD);  /* No offset */
    OLED_WR_Byte(0xD5, OLED_CMD);  /* Set OSC Frequency */
    OLED_WR_Byte(0x80, OLED_CMD);  /* ~100 FPS */
    OLED_WR_Byte(0xD9, OLED_CMD);  /* Set Pre-charge Period */
    OLED_WR_Byte(0xF1, OLED_CMD);  /* Pre-charge 15 clocks, Discharge 1 clock */
    OLED_WR_Byte(0xDA, OLED_CMD);  /* Set COM Pins Hardware Config */
    OLED_WR_Byte(0x12, OLED_CMD);
    OLED_WR_Byte(0xDB, OLED_CMD);  /* Set VCOMH Deselect Level */
    OLED_WR_Byte(0x40, OLED_CMD);
    OLED_WR_Byte(0x20, OLED_CMD);  /* Set Page Addressing Mode */
    OLED_WR_Byte(0x02, OLED_CMD);  /* Page addressing */
    OLED_WR_Byte(0x8D, OLED_CMD);  /* Set Charge Pump */
    OLED_WR_Byte(0x14, OLED_CMD);  /* Charge Pump ON (0x14=ON, 0x10=OFF) */
    OLED_WR_Byte(0xA4, OLED_CMD);  /* Display follows RAM (A4=normal, A5=all on) */
    OLED_WR_Byte(0xA6, OLED_CMD);  /* Normal (non-inverted) display */
    OLED_WR_Byte(0xAF, OLED_CMD);  /* Display ON */
    OLED_Clear();
    OLED_Set_Pos(0, 0);
}

/* 显示浮点数
 * (x,y): 起点坐标
 * num: 要显示的浮点数
 * int_digits: 整数部分显示位数
 * frac_digits: 小数部分显示位数
 * 实际显示位数 = int_digits + frac_digits + 2（含符号位和小数点）
 * 注意：float 精度有限，小数位数不宜超过 5~6 位
 */
void OLED_ShowFloat(u8 x, u8 y, float num, u8 int_digits, u8 frac_digits)
{
    if (num < 0)
    {
        OLED_ShowChar(x, y, '-');
        OLED_ShowNum((u8)(x + 8),  y, (u32)(-num), int_digits, 16);
        OLED_ShowChar((u8)(x + int_digits * 8 + 8),  y, '.');
        OLED_ShowNum((u8)(x + int_digits * 8 + 16), y,
                     (u32)((-num - (int32_t)(-num)) * oled_pow(10, frac_digits) + 0.5f),
                     frac_digits, 16);
    }
    else
    {
        OLED_ShowNum(x, y, (u32)num, int_digits + 1, 16);
        OLED_ShowChar((u8)(x + int_digits * 8 + 8),  y, '.');
        OLED_ShowNum((u8)(x + int_digits * 8 + 16), y,
                     (u32)((num - (int32_t)num) * oled_pow(10, frac_digits) + 0.5f),
                     frac_digits, 16);
    }
}
