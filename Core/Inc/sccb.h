#ifndef SCCB_H
#define SCCB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

/* 器件地址（8位写地址格式） */
#define OV2640_DEVICE_ADDRESS  0x60
#define OV5640_DEVICE_ADDRESS  0x78

/*---------- SCCB 总线引脚配置 ----------*/

#define SCCB_SCL_CLK_ENABLE  __HAL_RCC_GPIOB_CLK_ENABLE()
#define SCCB_SCL_PORT        GPIOB
#define SCCB_SCL_PIN         GPIO_PIN_8

#define SCCB_SDA_CLK_ENABLE  __HAL_RCC_GPIOB_CLK_ENABLE()
#define SCCB_SDA_PORT        GPIOB
#define SCCB_SDA_PIN         GPIO_PIN_9

/*---------- IIC 应答定义 ----------*/

#define ACK_OK   1   /* 应答正常 */
#define ACK_ERR  0   /* 应答错误 */

/* SCCB 通讯速度 — 约 300KHz */
#define SCCB_DelayVaule  8

/*---------- IO 操作宏 ----------*/

#define SCCB_SCL(a)  if (a) \
    HAL_GPIO_WritePin(SCCB_SCL_PORT, SCCB_SCL_PIN, GPIO_PIN_SET); \
  else \
    HAL_GPIO_WritePin(SCCB_SCL_PORT, SCCB_SCL_PIN, GPIO_PIN_RESET)

#define SCCB_SDA(a)  if (a) \
    HAL_GPIO_WritePin(SCCB_SDA_PORT, SCCB_SDA_PIN, GPIO_PIN_SET); \
  else \
    HAL_GPIO_WritePin(SCCB_SDA_PORT, SCCB_SDA_PIN, GPIO_PIN_RESET)

/*---------- 公共函数 ----------*/

void     SCCB_GPIO_Config(void);
void     SCCB_Delay(uint32_t a);
void     SCCB_Start(void);
void     SCCB_Stop(void);
void     SCCB_ACK(void);
void     SCCB_NoACK(void);
uint8_t  SCCB_WaitACK(void);
uint8_t  SCCB_WriteByte(uint8_t IIC_Data);
uint8_t  SCCB_ReadByte(uint8_t ACK_Mode);

/* 8位地址寄存器操作 — OV2640 用 */
uint8_t  SCCB_WriteReg(uint8_t addr, uint8_t value);
uint8_t  SCCB_ReadReg(uint8_t addr);

/* 16位地址寄存器操作 — OV5640 用 */
uint8_t  SCCB_WriteReg_16Bit(uint16_t addr, uint8_t value);
uint8_t  SCCB_ReadReg_16Bit(uint16_t addr);
uint8_t  SCCB_WriteBuffer_16Bit(uint16_t addr, uint8_t *pData, uint32_t size);

#ifdef __cplusplus
}
#endif

#endif /* SCCB_H */
