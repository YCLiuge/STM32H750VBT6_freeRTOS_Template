#include "app_sysmon.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "display_service.h"

#define SYSMON_SNAPSHOT_PERIOD_MS 1000U
#define SYSMON_LCD_PERIOD_MS      200U
#define RUNTIME_STATS_BUFFER_SIZE 768U
#define RUNTIME_STATS_MAX_TASKS   16U

static volatile uint32_t g_last_snapshot_tick = 0U;
static volatile uint32_t g_heap_free = 0U;
static volatile UBaseType_t g_wm_console = 0U;
static volatile UBaseType_t g_wm_sysmon  = 0U;
static volatile UBaseType_t g_wm_adc1    = 0U;
static volatile UBaseType_t g_wm_adc2    = 0U;
static volatile UBaseType_t g_wm_adc3    = 0U;

static char g_runtime_stats[RUNTIME_STATS_BUFFER_SIZE];

typedef struct
{
  TaskHandle_t handle;
  uint32_t prev_runtime;
  uint64_t accum_runtime;
} RuntimeStatsMirror_t;

static uint32_t g_runtime_prev_total = 0U;
static bool g_runtime_stats_primed = false;
static RuntimeStatsMirror_t g_runtime_mirror[RUNTIME_STATS_MAX_TASKS];

static void AppSysMon_LCDUpdateSnapshot(void);
static size_t AppSysMon_AppendFormat(char *buf, size_t buf_len, size_t used, const char *fmt, ...);

void AppSysMon_Task(void *argument)
{
  uint32_t last_snapshot_tick = 0U;

  (void)argument;

  for (;;)
  {
    uint32_t now = osKernelGetTickCount();

    if ((now - last_snapshot_tick) >= SYSMON_SNAPSHOT_PERIOD_MS)
    {
      last_snapshot_tick = now;
      AppSysMon_UpdateSnapshot();
    }

    AppSysMon_LCDUpdateSnapshot();
    osDelay(SYSMON_LCD_PERIOD_MS);
  }
}

void AppSysMon_UpdateSnapshot(void)
{
  g_last_snapshot_tick = HAL_GetTick();
  g_heap_free = xPortGetFreeHeapSize();

  g_wm_console = (Console_TaskHandle != NULL) ? uxTaskGetStackHighWaterMark((TaskHandle_t)Console_TaskHandle) : 0U;
  g_wm_sysmon  = (SysMon_TaskHandle  != NULL) ? uxTaskGetStackHighWaterMark((TaskHandle_t)SysMon_TaskHandle)  : 0U;
  g_wm_adc1    = (ADC1_TaskHandle    != NULL) ? uxTaskGetStackHighWaterMark((TaskHandle_t)ADC1_TaskHandle)    : 0U;
  g_wm_adc2    = (ADC2_TaskHandle    != NULL) ? uxTaskGetStackHighWaterMark((TaskHandle_t)ADC2_TaskHandle)    : 0U;
  g_wm_adc3    = (ADC3_TaskHandle    != NULL) ? uxTaskGetStackHighWaterMark((TaskHandle_t)ADC3_TaskHandle)    : 0U;
}

void AppSysMon_GetSnapshot(AppSystemSnapshot_t *snapshot)
{
  AppAdcStatus_t adc_status;
  AppConsoleStats_t console_stats;

  if (snapshot == NULL)
  {
    return;
  }

  memset(snapshot, 0, sizeof(*snapshot));
  AppAdc_GetStatus(&adc_status);
  AppConsole_GetStats(&console_stats);

  snapshot->tick_ms = g_last_snapshot_tick;
  snapshot->heap_free = g_heap_free;
  snapshot->adc1_avg = adc_status.adc1_avg;
  snapshot->adc2_avg = adc_status.adc2_avg;
  snapshot->adc1_cnt = adc_status.adc1_cnt;
  snapshot->adc2_cnt = adc_status.adc2_cnt;
  snapshot->adc3_cnt = adc_status.adc3_cnt;
  snapshot->temp_raw = adc_status.temp_raw;
  snapshot->vref_raw = adc_status.vref_raw;
  snapshot->vdda_mv = adc_status.vdda_mv;
  snapshot->temp_c_x10 = adc_status.temp_c_x10;
  snapshot->cli_cmd_cnt = console_stats.cli_cmd_cnt;
  snapshot->cli_drop_cnt = console_stats.cli_drop_cnt;
  snapshot->cli_err_cnt = console_stats.cli_err_cnt;
  snapshot->wm_console = g_wm_console;
  snapshot->wm_sysmon = g_wm_sysmon;
  snapshot->wm_adc1 = g_wm_adc1;
  snapshot->wm_adc2 = g_wm_adc2;
  snapshot->wm_adc3 = g_wm_adc3;
}

size_t AppSysMon_BuildSystemSummary(char *buf, size_t buf_len)
{
  AppSystemSnapshot_t snap;
  char temp_buf[24];
  size_t used = 0U;

  if ((buf == NULL) || (buf_len == 0U))
  {
    return 0U;
  }

  memset(buf, 0, buf_len);
  AppSysMon_GetSnapshot(&snap);
  AppAdc_FormatTemperatureX10(snap.temp_c_x10, temp_buf, sizeof(temp_buf));

  used = AppSysMon_AppendFormat(buf, buf_len, used,
      "[SYS]\r\n"
      "tick=%lu heap_free=%lu\r\n"
      "adc1_avg=%u adc2_avg=%u adc1_cnt=%lu adc2_cnt=%lu adc3_cnt=%lu\r\n"
      "temp=%s vdda=%lumV temp_raw=%lu vref_raw=%lu\r\n"
      "cli_cmd=%lu cli_drop=%lu cli_err=%lu\r\n",
      (unsigned long)snap.tick_ms,
      (unsigned long)snap.heap_free,
      (unsigned int)snap.adc1_avg,
      (unsigned int)snap.adc2_avg,
      (unsigned long)snap.adc1_cnt,
      (unsigned long)snap.adc2_cnt,
      (unsigned long)snap.adc3_cnt,
      temp_buf,
      (unsigned long)snap.vdda_mv,
      (unsigned long)snap.temp_raw,
      (unsigned long)snap.vref_raw,
      (unsigned long)snap.cli_cmd_cnt,
      (unsigned long)snap.cli_drop_cnt,
      (unsigned long)snap.cli_err_cnt);

  used = AppSysMon_AppendFormat(buf, buf_len, used,
      "watermark(words): console=%lu sysmon=%lu adc1=%lu adc2=%lu adc3=%lu\r\n",
      (unsigned long)snap.wm_console,
      (unsigned long)snap.wm_sysmon,
      (unsigned long)snap.wm_adc1,
      (unsigned long)snap.wm_adc2,
      (unsigned long)snap.wm_adc3);

  return used;
}

void AppSysMon_BuildRuntimeStatsDeltaReport(char *buf, size_t buf_len)
{
  TaskStatus_t task_status[RUNTIME_STATS_MAX_TASKS];
  UBaseType_t task_count;
  UBaseType_t i;
  unsigned long total_runtime_now = 0UL;
  uint32_t total_delta;
  size_t used = 0U;

  if ((buf == NULL) || (buf_len == 0U))
  {
    return;
  }

  memset(buf, 0, buf_len);
  task_count = uxTaskGetSystemState(task_status, RUNTIME_STATS_MAX_TASKS, &total_runtime_now);
  if (task_count == 0U)
  {
    (void)snprintf(buf, buf_len, "runtime delta unavailable\r\n");
    return;
  }

  total_delta = (uint32_t)(total_runtime_now - g_runtime_prev_total);
  if ((!g_runtime_stats_primed) || (total_delta == 0U))
  {
    for (i = 0U; i < task_count; i++)
    {
      g_runtime_mirror[i].handle = task_status[i].xHandle;
      g_runtime_mirror[i].prev_runtime = task_status[i].ulRunTimeCounter;
      g_runtime_mirror[i].accum_runtime = 0ULL;
    }

    for (; i < RUNTIME_STATS_MAX_TASKS; i++)
    {
      g_runtime_mirror[i].handle = NULL;
      g_runtime_mirror[i].prev_runtime = 0U;
      g_runtime_mirror[i].accum_runtime = 0ULL;
    }

    g_runtime_prev_total = (uint32_t)total_runtime_now;
    g_runtime_stats_primed = true;
    (void)snprintf(buf, buf_len, "runtime delta warm-up complete, run again for next window\r\n");
    return;
  }

  used = AppSysMon_AppendFormat(buf, buf_len, used, "TaskName        Delta      %%\r\n");

  for (i = 0U; (i < task_count) && (used < buf_len); i++)
  {
    RuntimeStatsMirror_t *slot = NULL;
    uint32_t task_delta = 0U;
    uint32_t percent = 0U;
    UBaseType_t j;

    for (j = 0U; j < RUNTIME_STATS_MAX_TASKS; j++)
    {
      if (g_runtime_mirror[j].handle == task_status[i].xHandle)
      {
        slot = &g_runtime_mirror[j];
        break;
      }
    }

    if (slot == NULL)
    {
      for (j = 0U; j < RUNTIME_STATS_MAX_TASKS; j++)
      {
        if (g_runtime_mirror[j].handle == NULL)
        {
          slot = &g_runtime_mirror[j];
          slot->handle = task_status[i].xHandle;
          slot->prev_runtime = task_status[i].ulRunTimeCounter;
          slot->accum_runtime = 0ULL;
          break;
        }
      }
    }

    if (slot != NULL)
    {
      task_delta = (uint32_t)(task_status[i].ulRunTimeCounter - slot->prev_runtime);
      slot->prev_runtime = task_status[i].ulRunTimeCounter;
      slot->accum_runtime += (uint64_t)task_delta;
    }

    percent = (total_delta != 0U) ? (uint32_t)(((uint64_t)task_delta * 100ULL) / (uint64_t)total_delta) : 0U;
    used = AppSysMon_AppendFormat(buf,
                                  buf_len,
                                  used,
                                  "%-15s %-10lu %3lu%%\r\n",
                                  task_status[i].pcTaskName,
                                  (unsigned long)task_delta,
                                  (unsigned long)percent);
  }

  g_runtime_prev_total = (uint32_t)total_runtime_now;
}

size_t AppSysMon_BuildCombinedReport(char *buf, size_t buf_len)
{
  size_t used;

  if ((buf == NULL) || (buf_len == 0U))
  {
    return 0U;
  }

  used = AppSysMon_BuildSystemSummary(buf, buf_len);
  used = AppSysMon_AppendFormat(buf, buf_len, used, "\r\n[Runtime Stats]\r\n");

  memset(g_runtime_stats, 0, sizeof(g_runtime_stats));
  vTaskGetRunTimeStats(g_runtime_stats);
  used = AppSysMon_AppendFormat(buf, buf_len, used, "%s", g_runtime_stats);

  return used;
}

static void AppSysMon_LCDUpdateSnapshot(void)
{
  AppAdcStatus_t adc_status;

  if (LCDMutexHandle == NULL)
  {
    return;
  }

  if (osMutexAcquire(LCDMutexHandle, 0U) != osOK)
  {
    return;
  }

  AppAdc_GetStatus(&adc_status);

  DisplayService_ShowNumber(8,  16, (int32_t)(adc_status.adc1_cnt % 100000000U), 8);
  DisplayService_ShowNumber(62, 16, (int32_t)adc_status.adc1_avg, 5);
  DisplayService_ShowNumber(8,  24, (int32_t)(adc_status.adc2_cnt % 100000000U), 8);
  DisplayService_ShowNumber(62, 24, (int32_t)adc_status.adc2_avg, 5);

  osMutexRelease(LCDMutexHandle);
}

static size_t AppSysMon_AppendFormat(char *buf, size_t buf_len, size_t used, const char *fmt, ...)
{
  va_list args;
  int written;

  if ((buf == NULL) || (buf_len == 0U) || (used >= buf_len))
  {
    return used;
  }

  va_start(args, fmt);
  written = vsnprintf(buf + used, buf_len - used, fmt, args);
  va_end(args);

  if (written < 0)
  {
    return used;
  }

  if ((size_t)written >= (buf_len - used))
  {
    return (buf_len - 1U);
  }

  return (used + (size_t)written);
}
