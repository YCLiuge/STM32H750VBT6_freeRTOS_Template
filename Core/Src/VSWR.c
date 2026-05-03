// VSWR.c
#include "VSWR.h"

// 初始化VSWR测量仪
void VSWR_Meter_Init(VSWR_Meter_t* meter, ADC_HandleTypeDef* hadc_fwd, 
                     ADC_HandleTypeDef* hadc_rev, TIM_HandleTypeDef* htim)
{
    meter->hadc_forward = hadc_fwd;
    meter->hadc_reverse = hadc_rev;
    meter->htim_adc_trigger = htim;
    
    // 默认校准参数
    meter->calibration.forward_offset = 0.0f;
    meter->calibration.reverse_offset = 0.0f;
    meter->calibration.forward_gain = 1.0f;
    meter->calibration.reverse_gain = 1.0f;
    meter->calibration.temperature_comp = 0.0f;
    
    meter->measurement_ready = 0;
    
    // 启动ADC触发定时器
    HAL_TIM_Base_Start(htim);
}

// 开始测量
void VSWR_Meter_StartMeasurement(VSWR_Meter_t* meter)
{
    // 启动ADC转换（使用双重模式同时采样）
    if (HAL_ADC_Start_DMA(meter->hadc_forward, (uint32_t*)meter->adc_buffer, 2) != HAL_OK)
    {
        Error_Handler();
    }
}

// 处理ADC数据并计算VSWR
void VSWR_Meter_ProcessData(VSWR_Meter_t* meter)
{
    // 将ADC值转换为电压（假设12位ADC，3.3V参考电压）
    float forward_voltage = (meter->adc_buffer[0] * 3.3f) / 4095.0f;
    float reverse_voltage = (meter->adc_buffer[1] * 3.3f) / 4095.0f;
    
    // 应用校准
    forward_voltage = (forward_voltage - meter->calibration.forward_offset) * meter->calibration.forward_gain;
    reverse_voltage = (reverse_voltage - meter->calibration.reverse_offset) * meter->calibration.reverse_gain;
    
    // 电压转换为功率（假设50欧姆系统，功率 = V2/R）
    // 实际应用中可能需要根据检波器特性进行转换
    meter->measurement.forward_power = (forward_voltage * forward_voltage) / 50.0f;
    meter->measurement.reverse_power = (reverse_voltage * reverse_voltage) / 50.0f;
    
    // 计算VSWR
    meter->measurement.vswr = VSWR_Meter_CalculateVSWR(
        meter->measurement.forward_power, 
        meter->measurement.reverse_power);
    
    // 计算回波损耗和反射系数
    meter->measurement.return_loss = VSWR_Meter_CalculateReturnLoss(meter->measurement.vswr);
    
    float gamma = sqrtf(meter->measurement.reverse_power / meter->measurement.forward_power);
    meter->measurement.reflection_coef = gamma;
    
    meter->measurement.timestamp = HAL_GetTick();
    meter->measurement_ready = 1;
}

// 计算驻波比
float VSWR_Meter_CalculateVSWR(float forward_power, float reverse_power)
{
    if (forward_power < 0.001f) return 999.9f; // 防止除零
    
    float gamma = sqrtf(reverse_power / forward_power);
    
    if (gamma >= 1.0f) return 999.9f; // 反射系数不能大于1
    
    float vswr = (1.0f + gamma) / (1.0f - gamma);
    
    return vswr;
}

// 计算回波损耗
float VSWR_Meter_CalculateReturnLoss(float vswr)
{
    if (vswr <= 1.0f) return 999.9f; // 完美匹配
    
    float gamma = (vswr - 1.0f) / (vswr + 1.0f);
    float return_loss_db = -20.0f * log10f(gamma);
    
    return return_loss_db;
}

// 校准函数
void VSWR_Meter_Calibrate(VSWR_Meter_t* meter, float known_forward, float known_reverse)
{
    // 读取当前ADC值
    VSWR_Meter_StartMeasurement(meter);
    HAL_Delay(10); // 等待转换完成
    
    float current_forward = (meter->adc_buffer[0] * 3.3f) / 4095.0f;
    float current_reverse = (meter->adc_buffer[1] * 3.3f) / 4095.0f;
    
    // 计算新的校准参数
    meter->calibration.forward_gain = known_forward / current_forward;
    meter->calibration.reverse_gain = known_reverse / current_reverse;
    
    // 可以保存到Flash
}

// 显示结果
void VSWR_Meter_DisplayResults(VSWR_Meter_t* meter)
{
    //if (!meter->measurement_ready) return;
    
    char buffer[128];
    
    snprintf(buffer, sizeof(buffer), 
             "VSWR: %.2f:1\r\n"
             "Forward: %.2f W\r\n"
             "Reverse: %.3f W\r\n"
             "Return Loss: %.1f dB\r\n"
             "Reflection: %.3f\r\n",
             meter->measurement.vswr,
             meter->measurement.forward_power,
             meter->measurement.reverse_power,
             meter->measurement.return_loss,
             meter->measurement.reflection_coef);
    LCD_DisplayString( 2 ,120, buffer);
    // 通过UART输出到显示屏
    HAL_UART_Transmit(meter->huart, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
    
    meter->measurement_ready = 0;
}