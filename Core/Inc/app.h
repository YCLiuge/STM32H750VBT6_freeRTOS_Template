#ifndef __APP_H__
#define __APP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "main.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "task.h"

/* =========================
 * Build-time display selection
 * Set to DISPLAY_TYPE_LCD or DISPLAY_TYPE_OLED
 * ========================= */
#define DISPLAY_TYPE_LCD  1
#define DISPLAY_TYPE_OLED 2
#ifndef APP_DISPLAY_TYPE
#define APP_DISPLAY_TYPE  DISPLAY_TYPE_OLED
#endif

/* =========================
 * Shared app-wide contracts
 * ========================= */
#define APP_ADC_DMA_SAMPLES 64U

typedef struct
{
  uint16_t adc1_avg;
  uint16_t adc2_avg;
  uint32_t adc1_cnt;
  uint32_t adc2_cnt;
  uint32_t adc3_cnt;
  uint32_t temp_raw;
  uint32_t vref_raw;
  uint32_t vdda_mv;
  int32_t  temp_c_x10;
} AppAdcStatus_t;

typedef struct
{
  uint32_t cli_cmd_cnt;
  uint32_t cli_drop_cnt;
  uint32_t cli_err_cnt;
} AppConsoleStats_t;

typedef struct
{
  uint32_t tick_ms;
  uint32_t heap_free;
  uint16_t adc1_avg;
  uint16_t adc2_avg;
  uint32_t adc1_cnt;
  uint32_t adc2_cnt;
  uint32_t adc3_cnt;
  uint32_t temp_raw;
  uint32_t vref_raw;
  uint32_t vdda_mv;
  int32_t  temp_c_x10;
  uint32_t cli_cmd_cnt;
  uint32_t cli_drop_cnt;
  uint32_t cli_err_cnt;
  UBaseType_t wm_console;
  UBaseType_t wm_sysmon;
  UBaseType_t wm_adc1;
  UBaseType_t wm_adc2;
  UBaseType_t wm_adc3;
} AppSystemSnapshot_t;

/* =========================
 * Shared RTOS objects
 * Defined in freertos.c
 * ========================= */
extern osThreadId_t Init_TaskHandle;
extern osThreadId_t SysMon_TaskHandle;
extern osThreadId_t ADC1_TaskHandle;
extern osThreadId_t ADC2_TaskHandle;
extern osThreadId_t ADC3_TaskHandle;
extern osThreadId_t Console_TaskHandle;

extern osMessageQueueId_t ADCData_QueueHandle;
extern osMutexId_t UsbTxMutexHandle;
extern osMutexId_t LCDMutexHandle;

/* =========================
 * Shared DMA sample buffers
 * Defined in app_adc.c
 * ========================= */
extern volatile uint16_t adc1_value[APP_ADC_DMA_SAMPLES];
extern volatile uint16_t adc2_value[APP_ADC_DMA_SAMPLES];

/* =========================
 * app_init
 * ========================= */
void AppInit_Task(void *argument);

/* =========================
 * app_adc
 * ========================= */
void AppAdc_Init(void);
void AppAdc_Task1(void *argument);
void AppAdc_Task2(void *argument);
void AppAdc_Task3(void *argument);
void AppAdc_DmaConvCpltCallback(ADC_HandleTypeDef *hadc);
void AppAdc_GetStatus(AppAdcStatus_t *status);
void AppAdc_GetRawPreview(uint16_t *adc1_dst, uint16_t *adc2_dst, uint32_t preview_len);
void AppAdc_FormatTemperatureX10(int32_t temp_x10, char *buf, size_t buf_len);

/* =========================
 * app_sysmon
 * ========================= */
void AppSysMon_Task(void *argument);
void AppSysMon_UpdateSnapshot(void);
void AppSysMon_GetSnapshot(AppSystemSnapshot_t *snapshot);
size_t AppSysMon_BuildSystemSummary(char *buf, size_t buf_len);
size_t AppSysMon_BuildCombinedReport(char *buf, size_t buf_len);
void AppSysMon_BuildRuntimeStatsDeltaReport(char *buf, size_t buf_len);

/* =========================
 * display_service
 * ========================= */
void DisplayService_Init(void);
void DisplayService_Clear(void);
void DisplayService_ShowText(uint16_t x, uint16_t y, const char *text);
void DisplayService_ShowNumber(uint16_t x, uint16_t y, int32_t num, uint8_t len);

/* =========================
 * app_camera
 * ========================= */
void AppCamera_Init(void);
bool AppCamera_IsInitialized(void);
uint16_t AppCamera_ReadID(void);
int8_t AppCamera_Snapshot(void);
int8_t AppCamera_Continuous(void);
void AppCamera_Stop(void);
uint8_t AppCamera_GetFrameState(void);
uint8_t AppCamera_GetFPS(void);
void AppCamera_ClearFrameState(void);
size_t AppCamera_GetBufferSize(void);
size_t AppCamera_GetWidth(void);
size_t AppCamera_GetHeight(void);
void AppCamera_HexDump(char *buf, size_t buf_len, uint32_t offset, uint32_t len);
void AppCamera_PixelSample(char *buf, size_t buf_len, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void AppCamera_BuildStatus(char *buf, size_t buf_len);
void AppCamera_BuildVerify(char *buf, size_t buf_len);

/* =========================
 * app_sd
 * ========================= */
bool AppSD_Mount(void);
void AppSD_MountAsync(void);
void AppSD_Umount(void);
bool AppSD_IsMounted(void);
size_t AppSD_ListDir(char *buf, size_t buf_len, const char *path);
size_t AppSD_ReadFile(char *buf, size_t buf_len, const char *path, uint32_t max_bytes);
size_t AppSD_BuildInfo(char *buf, size_t buf_len);

/* =========================
 * app_console
 * ========================= */
void AppConsole_Init(void);
void AppConsole_Task(void *argument);
void AppConsole_RxPushFromISR(const uint8_t *buf, uint32_t len);
void Console_RxPushFromISR(const uint8_t *buf, uint32_t len);
void AppConsole_GetStats(AppConsoleStats_t *stats);
void USBVcom_printf(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif /* __APP_H__ */
