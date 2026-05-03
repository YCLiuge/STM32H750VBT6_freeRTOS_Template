/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    VSWR.h
  * @brief   This file contains all the function prototypes for
  *          the VSWR.c file
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __VSWR_H__
#define __VSWR_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32h7xx_hal.h"
#include "spi.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// 测量参数结构体
typedef struct {
    float forward_power;    // 正向功率
    float reverse_power;    // 反向功率  
    float vswr;            // 驻波比
    float return_loss;     // 回波损耗
    float reflection_coef; // 反射系数
    uint32_t timestamp;
} VSWR_Measurement_t;

// 校准参数
typedef struct {
    float forward_offset;
    float reverse_offset;
    float forward_gain;
    float reverse_gain;
    float temperature_comp;
} CalibrationParams_t;

// VSWR测量仪类
typedef struct {
    ADC_HandleTypeDef* hadc_forward;
    ADC_HandleTypeDef* hadc_reverse;
    TIM_HandleTypeDef* htim_adc_trigger;
    UART_HandleTypeDef* huart;
    
    VSWR_Measurement_t measurement;
    CalibrationParams_t calibration;
    
    uint32_t adc_buffer[2];  // 0:正向, 1:反向
    uint8_t measurement_ready;
} VSWR_Meter_t;

// 函数声明
void VSWR_Meter_Init(VSWR_Meter_t* meter, ADC_HandleTypeDef* hadc_fwd, 
                     ADC_HandleTypeDef* hadc_rev, TIM_HandleTypeDef* htim);
void VSWR_Meter_StartMeasurement(VSWR_Meter_t* meter);
void VSWR_Meter_ProcessData(VSWR_Meter_t* meter);
void VSWR_Meter_Calibrate(VSWR_Meter_t* meter, float known_forward, float known_reverse);
float VSWR_Meter_CalculateVSWR(float forward_power, float reverse_power);
float VSWR_Meter_CalculateReturnLoss(float vswr);
void VSWR_Meter_DisplayResults(VSWR_Meter_t* meter);



#ifdef __cplusplus
}
#endif

#endif /* __VSWR_H__ */
