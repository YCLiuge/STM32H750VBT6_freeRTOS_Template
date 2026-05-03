#include "app_adc.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "adc.h"

#define DMA_NOCACHE_BASE_ADDR   0x30000000UL
#define DMA_NOCACHE_REGION_SIZE 0x00008000UL
#define ARRAY_SIZE(x)           (sizeof(x) / sizeof((x)[0]))
#define DMA_BUFFER_ATTR         __attribute__((section(".bss.dma_buffer"), aligned(32)))

DMA_BUFFER_ATTR volatile uint16_t adc1_value[APP_ADC_DMA_SAMPLES];
DMA_BUFFER_ATTR volatile uint16_t adc2_value[APP_ADC_DMA_SAMPLES];

static volatile uint16_t g_adc1_avg = 0U;
static volatile uint16_t g_adc2_avg = 0U;

static volatile uint32_t g_adc3_temp_raw = 0U;
static volatile uint32_t g_adc3_vref_raw = 0U;
static volatile uint32_t g_vdda_mv = 3300U;
static volatile int32_t  g_temp_c_x10 = INT32_MIN;

static volatile uint32_t g_adc1_cnt = 0U;
static volatile uint32_t g_adc2_cnt = 0U;
static volatile uint32_t g_adc3_cnt = 0U;

static bool g_adc1_dma_in_expected_nc = false;
static bool g_adc2_dma_in_expected_nc = false;

static bool AppAdc_BufferIsInExpectedNonCacheableRegion(const volatile void *addr, uint32_t len);
static void AppAdc_InvalidateDCacheByAddress(const volatile void *addr, uint32_t len);
static void AppAdc_CopySnapshot(volatile const uint16_t *src, uint16_t *dst, uint32_t len, bool dma_in_expected_nc);
static uint16_t AppAdc_AverageU16(const uint16_t *buf, uint32_t len);
static uint32_t AppAdc_ReadChannelRaw(uint32_t channel);
static void AppAdc_UpdateInternalParams(void);

void AppAdc_Init(void)
{
  g_adc1_dma_in_expected_nc = AppAdc_BufferIsInExpectedNonCacheableRegion(adc1_value, sizeof(adc1_value));
  g_adc2_dma_in_expected_nc = AppAdc_BufferIsInExpectedNonCacheableRegion(adc2_value, sizeof(adc2_value));

  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc1_value, (uint32_t)ARRAY_SIZE(adc1_value)) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2_value, (uint32_t)ARRAY_SIZE(adc2_value)) != HAL_OK)
  {
    Error_Handler();
  }

  AppAdc_UpdateInternalParams();
}

void AppAdc_Task1(void *argument)
{
  (void)argument;

  for (;;)
  {
    uint16_t adc_snapshot[APP_ADC_DMA_SAMPLES];

    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    AppAdc_CopySnapshot(adc1_value, adc_snapshot, APP_ADC_DMA_SAMPLES, g_adc1_dma_in_expected_nc);
    g_adc1_avg = AppAdc_AverageU16(adc_snapshot, APP_ADC_DMA_SAMPLES);
    ++g_adc1_cnt;
  }
}

void AppAdc_Task2(void *argument)
{
  (void)argument;

  for (;;)
  {
    uint16_t adc_snapshot[APP_ADC_DMA_SAMPLES];

    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    AppAdc_CopySnapshot(adc2_value, adc_snapshot, APP_ADC_DMA_SAMPLES, g_adc2_dma_in_expected_nc);
    g_adc2_avg = AppAdc_AverageU16(adc_snapshot, APP_ADC_DMA_SAMPLES);
    ++g_adc2_cnt;
  }
}

void AppAdc_Task3(void *argument)
{
  (void)argument;

  AppAdc_UpdateInternalParams();

  for (;;)
  {
    AppAdc_UpdateInternalParams();
    ++g_adc3_cnt;
    osDelay(1000U);
  }
}

void AppAdc_DmaConvCpltCallback(ADC_HandleTypeDef *hadc)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;

  if ((hadc->Instance == ADC1) && (ADC1_TaskHandle != NULL))
  {
    vTaskNotifyGiveFromISR((TaskHandle_t)ADC1_TaskHandle, &xHigherPriorityTaskWoken);
  }
  else if ((hadc->Instance == ADC2) && (ADC2_TaskHandle != NULL))
  {
    vTaskNotifyGiveFromISR((TaskHandle_t)ADC2_TaskHandle, &xHigherPriorityTaskWoken);
  }

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void AppAdc_GetStatus(AppAdcStatus_t *status)
{
  if (status == NULL)
  {
    return;
  }

  status->adc1_avg = g_adc1_avg;
  status->adc2_avg = g_adc2_avg;
  status->adc1_cnt = g_adc1_cnt;
  status->adc2_cnt = g_adc2_cnt;
  status->adc3_cnt = g_adc3_cnt;
  status->temp_raw = g_adc3_temp_raw;
  status->vref_raw = g_adc3_vref_raw;
  status->vdda_mv = g_vdda_mv;
  status->temp_c_x10 = g_temp_c_x10;
}

void AppAdc_GetRawPreview(uint16_t *adc1_dst, uint16_t *adc2_dst, uint32_t preview_len)
{
  if ((adc1_dst == NULL) || (adc2_dst == NULL) || (preview_len == 0U))
  {
    return;
  }

  AppAdc_CopySnapshot(adc1_value, adc1_dst, preview_len, g_adc1_dma_in_expected_nc);
  AppAdc_CopySnapshot(adc2_value, adc2_dst, preview_len, g_adc2_dma_in_expected_nc);
}

void AppAdc_FormatTemperatureX10(int32_t temp_x10, char *buf, size_t buf_len)
{
  if ((buf == NULL) || (buf_len == 0U))
  {
    return;
  }

  if (temp_x10 == INT32_MIN)
  {
    (void)snprintf(buf, buf_len, "n/a");
    return;
  }

  (void)snprintf(buf,
                 buf_len,
                 "%ld.%ldC",
                 (long)(temp_x10 / 10),
                 (long)((temp_x10 >= 0) ? (temp_x10 % 10) : -(temp_x10 % 10)));
}

static bool AppAdc_BufferIsInExpectedNonCacheableRegion(const volatile void *addr, uint32_t len)
{
  uintptr_t start = (uintptr_t)addr;
  uintptr_t end = start + (uintptr_t)len;
  uintptr_t region_end = DMA_NOCACHE_BASE_ADDR + DMA_NOCACHE_REGION_SIZE;

  return ((start >= DMA_NOCACHE_BASE_ADDR) && (end <= region_end));
}

static void AppAdc_InvalidateDCacheByAddress(const volatile void *addr, uint32_t len)
{
  uintptr_t aligned_addr;
  uint32_t aligned_len;

  aligned_addr = ((uintptr_t)addr) & ~(uintptr_t)0x1FUL;
  aligned_len = (uint32_t)((((uintptr_t)addr + (uintptr_t)len + 31UL) & ~(uintptr_t)0x1FUL) - aligned_addr);

  SCB_InvalidateDCache_by_Addr((uint32_t *)aligned_addr, (int32_t)aligned_len);
  __DSB();
  __ISB();
}

static void AppAdc_CopySnapshot(volatile const uint16_t *src, uint16_t *dst, uint32_t len, bool dma_in_expected_nc)
{
  uint32_t i;

  if ((!dma_in_expected_nc) && (len > 0U))
  {
    AppAdc_InvalidateDCacheByAddress(src, len * sizeof(src[0]));
  }

  for (i = 0U; i < len; i++)
  {
    dst[i] = src[i];
  }
}

static uint16_t AppAdc_AverageU16(const uint16_t *buf, uint32_t len)
{
  uint32_t sum = 0U;
  uint32_t i;

  if ((buf == NULL) || (len == 0U))
  {
    return 0U;
  }

  for (i = 0U; i < len; i++)
  {
    sum += buf[i];
  }

  return (uint16_t)(sum / len);
}

static uint32_t AppAdc_ReadChannelRaw(uint32_t channel)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  uint32_t raw = 0U;

  (void)HAL_ADC_Stop(&hadc3);

  sConfig.Channel = channel;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_810CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  sConfig.OffsetSignedSaturation = DISABLE;

  if (HAL_ADC_ConfigChannel(&hadc3, &sConfig) != HAL_OK)
  {
    return 0U;
  }

  if (HAL_ADC_Start(&hadc3) != HAL_OK)
  {
    return 0U;
  }

  if (HAL_ADC_PollForConversion(&hadc3, 20U) == HAL_OK)
  {
    raw = HAL_ADC_GetValue(&hadc3);
  }

  (void)HAL_ADC_Stop(&hadc3);
  return raw;
}

static void AppAdc_UpdateInternalParams(void)
{
  uint32_t temp_raw;
  uint32_t vref_raw;
  uint32_t vref_cal;
  uint32_t ts_cal1;
  uint32_t ts_cal2;
  int32_t temp_c_x10;

  temp_raw = AppAdc_ReadChannelRaw(ADC_CHANNEL_TEMPSENSOR);
  vref_raw = AppAdc_ReadChannelRaw(ADC_CHANNEL_VREFINT);

  g_adc3_temp_raw = temp_raw;
  g_adc3_vref_raw = vref_raw;

  vref_cal = *((uint16_t *)VREFINT_CAL_ADDR);
  if ((vref_raw != 0U) && (vref_cal != 0U))
  {
    g_vdda_mv = (uint32_t)((3300UL * vref_cal) / vref_raw);
  }

  ts_cal1 = *((uint16_t *)TEMPSENSOR_CAL1_ADDR);
  ts_cal2 = *((uint16_t *)TEMPSENSOR_CAL2_ADDR);
  if ((ts_cal2 > ts_cal1) && (g_vdda_mv > 0U))
  {
    int32_t compensated_raw;
    compensated_raw = (int32_t)((temp_raw * 3300UL) / g_vdda_mv);
    temp_c_x10 = (((compensated_raw - (int32_t)ts_cal1) * (1300 - 300)) /
                  ((int32_t)ts_cal2 - (int32_t)ts_cal1)) + 300;
    g_temp_c_x10 = temp_c_x10;
  }
}
