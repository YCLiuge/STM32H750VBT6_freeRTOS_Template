#include "display_service.h"

#include <string.h>

#if (APP_DISPLAY_TYPE == DISPLAY_TYPE_LCD)
/*
 * LCD backend: delegates to the ST7789 driver in spi.c (USER CODE region).
 * Functions: SPI_LCD_Init, LCD_Clear, LCD_DisplayString, LCD_DisplayNumber
 * Declared in spi.h (CubeMX-generated, LCD functions added in USER CODE).
 */
#include "lcd_st7789.h"

void DisplayService_Init(void)
{
  SPI_LCD_Init();
}

void DisplayService_Clear(void)
{
  LCD_Clear();
}

void DisplayService_ShowText(uint16_t x, uint16_t y, const char *text)
{
  if (text != NULL)
  {
    LCD_DisplayString(x, y, (char *)text);
  }
}

void DisplayService_ShowNumber(uint16_t x, uint16_t y, int32_t num, uint8_t len)
{
  LCD_DisplayNumber(x, y, num, len);
}

#elif (APP_DISPLAY_TYPE == DISPLAY_TYPE_OLED)
/*
 * OLED backend: delegates to the SSD1306 driver in oled_spi_V0.2.c.
 * SSD1306 128x64: Y is page-addressed (8 pages × 8 px).
 * Input y is pixel-relative (0–63); internally converted to page = y / 8.
 * Font: SIZE=12 → 6×8 px per character.
 */
#include "oled_spi_V0.2.h"

static u8 to_page(uint16_t pixel_y)
{
  u8 page = (u8)(pixel_y / 8U);
  if (page > 7U) { page = 7U; }
  return page;
}

void DisplayService_Init(void)
{
  OLED_Init();
}

void DisplayService_Clear(void)
{
  OLED_Clear();
}

void DisplayService_ShowText(uint16_t x, uint16_t y, const char *text)
{
  if (text != NULL)
  {
    OLED_ShowString((u8)x, to_page(y), (char *)text);
  }
}

void DisplayService_ShowNumber(uint16_t x, uint16_t y, int32_t num, uint8_t len)
{
  u8 page = to_page(y);
  u32 display_val;
  char sign_char;

  if (num < 0)
  {
    sign_char = '-';
    display_val = (u32)(-num);
  }
  else
  {
    sign_char = ' ';
    display_val = (u32)num;
  }

  if ((x >= 8U) && (len > 1U))
  {
    OLED_ShowString((u8)(x - 8U), page, &sign_char);
    OLED_ShowNum((u8)x, page, display_val, (u8)(len - 1U), 12U);
  }
  else
  {
    OLED_ShowNum((u8)x, page, display_val, (u8)len, 12U);
  }
}

#else
#error "APP_DISPLAY_TYPE must be DISPLAY_TYPE_LCD or DISPLAY_TYPE_OLED"
#endif
