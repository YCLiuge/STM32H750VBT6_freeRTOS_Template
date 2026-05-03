/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    dcmi.h
  * @brief   This file contains all the function prototypes for
  *          the dcmi.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __DCMI_H__
#define __DCMI_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

#include "sccb.h"
#include "lcd_st7789.h"
/* USER CODE END Includes */

extern DCMI_HandleTypeDef hdcmi;

/* USER CODE BEGIN Private defines */

/* DCMI 状态标志 — 帧传输完成时由 HAL_DCMI_FrameEventCallback() 中断回调置位 */
extern volatile uint8_t DCMI_FrameState;  /* 帧完成标志，在中断回调函数中置位 */
extern volatile uint8_t OV2640_FPS;       /* 帧率 (FPS) */

#define OV2640_Success   0                /* 通讯成功标志 */
#define OV2640_Error     -1               /* 通讯错误 */

#define OV2640_Enable    1
#define OV2640_Disable   0

/* 摄像头输出图像格式 — 参考 OV2640_Set_Pixformat() 函数 */
#define Pixformat_RGB565  0
#define Pixformat_JPEG    1

/* OV2640 特效模式 — 参考 OV2640_Set_Effect() 函数 */
#define OV2640_Effect_Normal       0       /* 普通模式 */
#define OV2640_Effect_Negative     1       /* 负片模式（颜色全取反）*/
#define OV2640_Effect_BW           2       /* 黑白模式 */
#define OV2640_Effect_BW_Negative  3       /* 黑白 + 负片模式 */

/*
 * 摄像头图像大小设置:
 *   1. 可根据实际应用环境或显示范围进行调整，同时需修改 PCLK 时钟分频
 *   2. 图像大小会影响帧率，且不能超过对应模式的最大尺寸
 *   3. SVGA 模式: 最大图像分辨率为 800×600, 最高 30fps
 *   4. UXGA 模式: 最大图像分辨率为 1600×1200, 最高 15fps
 *   5. 长宽必须能被 4 整除
 *   6. 长宽比必须为 4:3，否则会被裁剪
 */
#define OV2640_Width           400         /* 图像宽度 */
#define OV2640_Height          300         /* 图像高度 */

/*
 * 显示窗口大小:
 *   1. 数值必须能被 4 整除
 *   2. RGB565 模式下，DCMI 会将 OV2640 的 4:3 图像裁剪为显示窗口比例
 *   3. 分辨率不能超过 OV2640_Width × OV2640_Height
 *   4. 分辨率过高时需调整 PCLK，详见 dcmi_ov2640_cfg.h 的 0xD3 寄存器说明
 */
#define Display_Width          LCD_Width   /* 依赖 LCD 驱动中定义的实际屏幕宽 */
#define Display_Height         LCD_Height  /* 依赖 LCD 驱动中定义的实际屏幕高 */

/*
 * DMA 缓冲区大小 (32位字):
 *   RGB565: OV2640_BufferSize = Display_Width × Display_Height × 2 / 4
 *   JPEG:   需要约 30KB (640×480)，预留 2 倍即可，用户根据实际调整
 */
#define OV2640_BufferSize     (Display_Width * Display_Height * 2 / 4)

#define OV2640_SEL_Registers     0xFF      /* 寄存器组选择寄存器 */
#define OV2640_SEL_DSP           0x00      /* 0x00: 选择 DSP 寄存器组 */
#define OV2640_SEL_SENSOR        0x01      /* 0x01: 选择 SENSOR 寄存器组 */

/* DSP 寄存器组 (0xFF = 0x00) */
#define OV2640_DSP_RESET         0xE0      /* 复位 SCCB/JPEG/DVP 接口单元 */
#define OV2640_DSP_BPADDR        0x7C      /* 间接寄存器: 地址 */
#define OV2640_DSP_BPDATA        0x7D      /* 间接寄存器: 数据 */

/* SENSOR 寄存器组 (0xFF = 0x01) */
#define OV2640_SENSOR_COM7       0x12      /* COM7: 系统复位 + 分辨率 + 彩色条模式 */
#define OV2640_SENSOR_REG04      0x04      /* REG04: 图像窗口行扫描方向 */
#define OV2640_SENSOR_PIDH       0x0A      /* ID 高字节 */
#define OV2640_SENSOR_PIDL       0x0B      /* ID 低字节 */

/*------------------------------------------------------------ 摄像头控制函数 ------------------------------------------------*/

int8_t   DCMI_OV2640_Init(void);           /* 初始化 SCCB + DCMI + DMA + OV2640 */

void     OV2640_DMA_Transmit_Continuous(uint32_t DMA_Buffer, uint32_t DMA_BufferSize);  /* 连续模式 DMA */
void     OV2640_DMA_Transmit_Snapshot(uint32_t DMA_Buffer, uint32_t DMA_BufferSize);    /* 快照模式 DMA (抓一帧停) */
void     OV2640_DCMI_Suspend(void);        /* 暂停 DCMI (停止捕获) */
void     OV2640_DCMI_Resume(void);         /* 恢复 DCMI (开始捕获) */
void     OV2640_DCMI_Stop(void);           /* 停止 DCMI + DMA, 禁止 DCMI 中断 */
int8_t   OV2640_DCMI_Crop(uint16_t Display_XSize, uint16_t Display_YSize,
                          uint16_t Sensor_XSize, uint16_t Sensor_YSize);  /* 裁剪图像 */

void     OV2640_Reset(void);               /* 执行硬件复位 */
uint16_t OV2640_ReadID(void);              /* 读取摄像头 ID */
void     OV2640_Config(const uint8_t (*ConfigData)[2]);  /* 加载配置表 */
void     OV2640_Set_Pixformat(uint8_t pixformat);        /* 设置输出图像格式 */
int8_t   OV2640_Set_Framesize(uint16_t width, uint16_t height);  /* 设置实际输出的图像大小 */
int8_t   OV2640_Set_Horizontal_Mirror(int8_t ConfigState);       /* 水平镜像 */
int8_t   OV2640_Set_Vertical_Flip(int8_t ConfigState);           /* 垂直翻转 */
void     OV2640_Set_Saturation(int8_t Saturation);               /* 设置饱和度 */
void     OV2640_Set_Brightness(int8_t Brightness);               /* 设置亮度 */
void     OV2640_Set_Contrast(int8_t Contrast);                   /* 设置对比度 */
void     OV2640_Set_Effect(uint8_t effect_Mode);                 /* 设置特效 (负片/黑白/黑白+负片) */

/*-------------------------------------------------------------- 摄像头硬件配置宏 ---------------------------------------------*/

#define OV2640_PWDN_PIN                   GPIO_PIN_14                /* PWDN 引脚 */
#define OV2640_PWDN_PORT                  GPIOD                      /* PWDN GPIO 端口 */
#define GPIO_OV2640_PWDN_CLK_ENABLE       __HAL_RCC_GPIOD_CLK_ENABLE()  /* PWDN GPIO 时钟 */

/* PWDN 电平控制:
 *   LOW  = 正常模式, 摄像头开始工作
 *   HIGH = 省电模式, 摄像头停止工作 (最大限度省电)
 */
#define OV2640_PWDN_OFF   HAL_GPIO_WritePin(OV2640_PWDN_PORT, OV2640_PWDN_PIN, GPIO_PIN_RESET)
#define OV2640_PWDN_ON    HAL_GPIO_WritePin(OV2640_PWDN_PORT, OV2640_PWDN_PIN, GPIO_PIN_SET)
/* USER CODE END Private defines */

void MX_DCMI_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __DCMI_H__ */

