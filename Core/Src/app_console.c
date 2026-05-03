#include "app_console.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stream_buffer.h"
#include "FreeRTOS_CLI.h"
#include "usbd_cdc_if.h"

#define CONSOLE_RX_STREAM_SIZE   256U
#define CONSOLE_INPUT_MAX_LEN    96U
#define CONSOLE_OUTPUT_MAX_LEN   768U
#define CONSOLE_PROMPT           "> "
#define USB_PRINTF_BUFFER_SIZE   1024U
#define USB_TX_BUSY_TIMEOUT_MS   20U
#define DIAG_REPORT_BUFFER_SIZE  2048U
#define ARRAY_SIZE(x)            (sizeof(x) / sizeof((x)[0]))
#define MIN_U32(a, b)            (((a) < (b)) ? (a) : (b))

static StaticStreamBuffer_t g_console_rx_stream_struct;
static uint8_t g_console_rx_stream_storage[CONSOLE_RX_STREAM_SIZE];
static StreamBufferHandle_t g_console_rx_stream = NULL;

static volatile uint32_t g_console_rx_drop_cnt = 0U;
static volatile uint32_t g_console_cmd_cnt = 0U;
static volatile uint32_t g_console_err_cnt = 0U;

static uint8_t g_usb_printf_buf[USB_PRINTF_BUFFER_SIZE];
static char g_diag_report[DIAG_REPORT_BUFFER_SIZE];
static bool g_cli_registered = false;

static void AppConsole_Write(const char *str);
static void AppConsole_WritePrompt(void);
static void AppConsole_NormalizeLine(char *line);
static void AppConsole_ProcessLine(char *line);
static void AppConsole_RegisterCommands(void);

static BaseType_t CLI_HelpCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t CLI_TaskListCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t CLI_RunTimeStatsCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t CLI_RunTimeDeltaCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t CLI_SysCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t CLI_AllCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t CLI_AdcCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t CLI_CameraCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);
static BaseType_t CLI_SdCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString);

static const CLI_Command_Definition_t xHelpCmd = {
  "help",
  "help: Show available commands\r\n",
  CLI_HelpCommand,
  0
};

static const CLI_Command_Definition_t xTaskListCmd = {
  "tasks",
  "tasks: Show task state table\r\n",
  CLI_TaskListCommand,
  0
};

static const CLI_Command_Definition_t xRunTimeStatsCmd = {
  "stats",
  "stats: Show run-time statistics\r\n",
  CLI_RunTimeStatsCommand,
  0
};

static const CLI_Command_Definition_t xRunTimeDeltaCmd = {
  "statsd",
  "statsd: Show run-time delta statistics between two invocations\r\n",
  CLI_RunTimeDeltaCommand,
  0
};

static const CLI_Command_Definition_t xSysCmd = {
  "sys",
  "sys: Show cached system snapshot\r\n",
  CLI_SysCommand,
  0
};

static const CLI_Command_Definition_t xAllCmd = {
  "all",
  "all: Show system snapshot followed by run-time statistics\r\n",
  CLI_AllCommand,
  0
};

static const CLI_Command_Definition_t xAdcCmd = {
  "adc",
  "adc [raw]: Show ADC summary or first few DMA samples\r\n",
  CLI_AdcCommand,
  -1
};

static const CLI_Command_Definition_t xCameraCmd = {
  "camera",
  "camera: Camera diagnostics\r\n"
  "  camera id       - Read OV2640 sensor ID via SCCB\r\n"
  "  camera init     - Full camera init (SCCB + OV2640 config)\r\n"
  "  camera snap     - Capture one frame snapshot + hex preview\r\n"
  "  camera cont     - Start continuous capture\r\n"
  "  camera stop     - Stop DCMI capture\r\n"
  "  camera status   - Show camera status (FPS, frame state)\r\n"
  "  camera hex      - Hex dump from camera buffer\r\n"
  "  camera pixels   - Sample a pixel region (e.g. camera pixels 10 10 4 4)\r\n"
  "  camera verify   - Full image statistics (min/max/avg/zero/sat + center 5x5)\r\n",
  CLI_CameraCommand,
  -1
};

static const CLI_Command_Definition_t xSdCmd = {
  "sd",
  "sd: SD card operations\r\n"
  "  sd mount   - Initialize SD card and mount FATFS\r\n"
  "  sd umount  - Unmount FATFS\r\n"
  "  sd info    - Show SD card info (capacity, free space)\r\n"
  "  sd dir     - List root directory\r\n"
  "  sd cat <f> - Read and display text file content (max 2KB)\r\n",
  CLI_SdCommand,
  -1
};

void AppConsole_Init(void)
{
  if (g_console_rx_stream == NULL)
  {
    g_console_rx_stream = xStreamBufferCreateStatic(
        CONSOLE_RX_STREAM_SIZE,
        1U,
        g_console_rx_stream_storage,
        &g_console_rx_stream_struct);
  }
}

void AppConsole_Task(void *argument)
{
  uint8_t ch;
  size_t idx = 0U;
  char input_line[CONSOLE_INPUT_MAX_LEN];

  (void)argument;
  memset(input_line, 0, sizeof(input_line));

  AppConsole_RegisterCommands();
  AppConsole_Write("\r\nSTM32H750 FreeRTOS CLI ready. Type 'help'.\r\n");
  AppConsole_WritePrompt();

  for (;;)
  {
    if ((g_console_rx_stream == NULL) ||
        (xStreamBufferReceive(g_console_rx_stream, &ch, 1U, portMAX_DELAY) != 1U))
    {
      continue;
    }

    if ((ch == '\r') || (ch == '\n'))
    {
      AppConsole_Write("\r\n");

      if (idx > 0U)
      {
        input_line[idx] = '\0';
        AppConsole_ProcessLine(input_line);
        memset(input_line, 0, sizeof(input_line));
        idx = 0U;
      }

      AppConsole_WritePrompt();
      continue;
    }

    if ((ch == '\b') || (ch == 0x7FU))
    {
      if (idx > 0U)
      {
        idx--;
        input_line[idx] = '\0';
        AppConsole_Write("\b \b");
      }
      continue;
    }

    if (isprint((int)ch) != 0)
    {
      if (idx < (sizeof(input_line) - 1U))
      {
        char echo_buf[2];
        input_line[idx++] = (char)ch;
        echo_buf[0] = (char)ch;
        echo_buf[1] = '\0';
        AppConsole_Write(echo_buf);
      }
      else
      {
        ++g_console_err_cnt;
        AppConsole_Write("\a");
      }
    }
  }
}

void AppConsole_RxPushFromISR(const uint8_t *buf, uint32_t len)
{
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  size_t written;

  if ((g_console_rx_stream == NULL) || (buf == NULL) || (len == 0U))
  {
    return;
  }

  written = xStreamBufferSendFromISR(g_console_rx_stream, buf, len, &xHigherPriorityTaskWoken);

  if (written < len)
  {
    g_console_rx_drop_cnt += (uint32_t)(len - written);
  }

  portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void Console_RxPushFromISR(const uint8_t *buf, uint32_t len)
{
  AppConsole_RxPushFromISR(buf, len);
}

void AppConsole_GetStats(AppConsoleStats_t *stats)
{
  if (stats == NULL)
  {
    return;
  }

  stats->cli_cmd_cnt = g_console_cmd_cnt;
  stats->cli_drop_cnt = g_console_rx_drop_cnt;
  stats->cli_err_cnt = g_console_err_cnt;
}

void USBVcom_printf(const char *format, ...)
{
  int len;
  uint32_t start_tick;
  va_list args;

  if ((format == NULL) || (UsbTxMutexHandle == NULL) || (osKernelGetState() != osKernelRunning))
  {
    return;
  }

  if (osMutexAcquire(UsbTxMutexHandle, osWaitForever) != osOK)
  {
    return;
  }

  va_start(args, format);
  len = vsnprintf((char *)g_usb_printf_buf, sizeof(g_usb_printf_buf), format, args);
  va_end(args);

  if (len <= 0)
  {
    (void)osMutexRelease(UsbTxMutexHandle);
    return;
  }

  if (len >= (int)sizeof(g_usb_printf_buf))
  {
    len = (int)sizeof(g_usb_printf_buf) - 1;
  }

  if (!CDC_IsReady_FS())
  {
    (void)osMutexRelease(UsbTxMutexHandle);
    return;
  }

  start_tick = osKernelGetTickCount();
  while (CDC_Transmit_FS((uint8_t *)g_usb_printf_buf, (uint16_t)len) == USBD_BUSY)
  {
    if ((osKernelGetTickCount() - start_tick) > USB_TX_BUSY_TIMEOUT_MS)
    {
      ++g_console_err_cnt;
      break;
    }
    osDelay(1U);
  }

  (void)osMutexRelease(UsbTxMutexHandle);
}

static void AppConsole_Write(const char *str)
{
  if ((str != NULL) && (str[0] != '\0'))
  {
    USBVcom_printf("%s", str);
  }
}

static void AppConsole_WritePrompt(void)
{
  AppConsole_Write(CONSOLE_PROMPT);
}

static void AppConsole_NormalizeLine(char *line)
{
  size_t len;
  char *start;

  if (line == NULL)
  {
    return;
  }

  start = line;
  while ((*start != '\0') && (isspace((unsigned char)*start) != 0))
  {
    start++;
  }

  if (start != line)
  {
    memmove(line, start, strlen(start) + 1U);
  }

  len = strlen(line);
  while ((len > 0U) && (isspace((unsigned char)line[len - 1U]) != 0))
  {
    line[len - 1U] = '\0';
    len--;
  }
}

static void AppConsole_RegisterCommands(void)
{
  if (g_cli_registered)
  {
    return;
  }

  (void)FreeRTOS_CLIRegisterCommand(&xHelpCmd);
  (void)FreeRTOS_CLIRegisterCommand(&xTaskListCmd);
  (void)FreeRTOS_CLIRegisterCommand(&xRunTimeStatsCmd);
  (void)FreeRTOS_CLIRegisterCommand(&xRunTimeDeltaCmd);
  (void)FreeRTOS_CLIRegisterCommand(&xSysCmd);
  (void)FreeRTOS_CLIRegisterCommand(&xAllCmd);
  (void)FreeRTOS_CLIRegisterCommand(&xAdcCmd);
  (void)FreeRTOS_CLIRegisterCommand(&xCameraCmd);
  (void)FreeRTOS_CLIRegisterCommand(&xSdCmd);
  g_cli_registered = true;
}

static void AppConsole_ProcessLine(char *line)
{
  BaseType_t xMoreDataToFollow;
  char output[CONSOLE_OUTPUT_MAX_LEN];

  if (line == NULL)
  {
    return;
  }

  AppConsole_NormalizeLine(line);
  if (line[0] == '\0')
  {
    return;
  }

  ++g_console_cmd_cnt;

  do
  {
    memset(output, 0, sizeof(output));
    xMoreDataToFollow = FreeRTOS_CLIProcessCommand(line, output, sizeof(output));

    if (output[0] != '\0')
    {
      AppConsole_Write(output);
    }
    else if (xMoreDataToFollow == pdFALSE)
    {
      ++g_console_err_cnt;
    }
  } while (xMoreDataToFollow != pdFALSE);
}

static BaseType_t CLI_HelpCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
  (void)pcCommandString;

  if ((pcWriteBuffer == NULL) || (xWriteBufferLen == 0U))
  {
    return pdFALSE;
  }

  (void)snprintf(
      pcWriteBuffer,
      xWriteBufferLen,
      "Available commands:\r\n"
      "  help   - list commands\r\n"
      "  sys    - cached system snapshot\r\n"
      "  stats  - FreeRTOS run-time statistics\r\n"
      "  statsd - delta statistics between two calls\r\n"
      "  all    - system snapshot + run-time statistics\r\n"
      "  tasks  - task list\r\n"
      "  adc    - ADC summary\r\n"
      "  adc raw- first few DMA samples\r\n");

  return pdFALSE;
}

static BaseType_t CLI_TaskListCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
  (void)pcCommandString;

  if ((pcWriteBuffer == NULL) || (xWriteBufferLen == 0U))
  {
    return pdFALSE;
  }

  memset(pcWriteBuffer, 0, xWriteBufferLen);
  vTaskList(pcWriteBuffer);
  return pdFALSE;
}

static BaseType_t CLI_RunTimeStatsCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
  (void)pcCommandString;

  if ((pcWriteBuffer == NULL) || (xWriteBufferLen == 0U))
  {
    return pdFALSE;
  }

  memset(pcWriteBuffer, 0, xWriteBufferLen);
  vTaskGetRunTimeStats(pcWriteBuffer);
  return pdFALSE;
}

static BaseType_t CLI_RunTimeDeltaCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
  (void)pcCommandString;

  if ((pcWriteBuffer == NULL) || (xWriteBufferLen == 0U))
  {
    return pdFALSE;
  }

  AppSysMon_BuildRuntimeStatsDeltaReport(pcWriteBuffer, xWriteBufferLen);
  return pdFALSE;
}

static BaseType_t CLI_SysCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
  (void)pcCommandString;

  if ((pcWriteBuffer == NULL) || (xWriteBufferLen == 0U))
  {
    return pdFALSE;
  }

  (void)AppSysMon_BuildSystemSummary(pcWriteBuffer, xWriteBufferLen);
  return pdFALSE;
}

static BaseType_t CLI_AllCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
  static size_t s_offset = 0U;
  size_t total_len;
  size_t copy_len;

  (void)pcCommandString;

  if ((pcWriteBuffer == NULL) || (xWriteBufferLen == 0U))
  {
    s_offset = 0U;
    return pdFALSE;
  }

  if (s_offset == 0U)
  {
    (void)AppSysMon_BuildCombinedReport(g_diag_report, sizeof(g_diag_report));
  }

  total_len = strlen(g_diag_report);
  if (s_offset >= total_len)
  {
    s_offset = 0U;
    pcWriteBuffer[0] = '\0';
    return pdFALSE;
  }

  copy_len = MIN_U32((uint32_t)(xWriteBufferLen - 1U), (uint32_t)(total_len - s_offset));
  memcpy(pcWriteBuffer, &g_diag_report[s_offset], copy_len);
  pcWriteBuffer[copy_len] = '\0';
  s_offset += copy_len;

  if (s_offset < total_len)
  {
    return pdTRUE;
  }

  s_offset = 0U;
  return pdFALSE;
}

static BaseType_t CLI_AdcCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
  const char *param1;
  BaseType_t param1_len = 0;

  if ((pcWriteBuffer == NULL) || (xWriteBufferLen == 0U))
  {
    return pdFALSE;
  }

  param1 = FreeRTOS_CLIGetParameter(pcCommandString, 1U, &param1_len);

  if ((param1 != NULL) && (param1_len == 3) && (strncmp(param1, "raw", 3) == 0))
  {
    uint16_t adc1_preview[5];
    uint16_t adc2_preview[5];

    AppAdc_GetRawPreview(adc1_preview, adc2_preview, (uint32_t)ARRAY_SIZE(adc1_preview));

    (void)snprintf(
        pcWriteBuffer,
        xWriteBufferLen,
        "adc1_raw=[%u,%u,%u,%u,%u] adc2_raw=[%u,%u,%u,%u,%u]\r\n",
        adc1_preview[0], adc1_preview[1], adc1_preview[2], adc1_preview[3], adc1_preview[4],
        adc2_preview[0], adc2_preview[1], adc2_preview[2], adc2_preview[3], adc2_preview[4]);
  }
  else
  {
    AppAdcStatus_t adc_status;
    char temp_buf[24];

    AppAdc_GetStatus(&adc_status);
    AppAdc_FormatTemperatureX10(adc_status.temp_c_x10, temp_buf, sizeof(temp_buf));

    (void)snprintf(
        pcWriteBuffer,
        xWriteBufferLen,
        "adc1_avg=%u adc2_avg=%u temp=%s temp_raw=%lu vref_raw=%lu vdda=%lumV\r\n",
        (unsigned int)adc_status.adc1_avg,
        (unsigned int)adc_status.adc2_avg,
        temp_buf,
        (unsigned long)adc_status.temp_raw,
        (unsigned long)adc_status.vref_raw,
        (unsigned long)adc_status.vdda_mv);
  }

  return pdFALSE;
}

static BaseType_t CLI_CameraCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
  const char *subcmd;
  BaseType_t subcmd_len = 0;

  if ((pcWriteBuffer == NULL) || (xWriteBufferLen == 0U))
  {
    return pdFALSE;
  }

  subcmd = FreeRTOS_CLIGetParameter(pcCommandString, 1U, &subcmd_len);

  if ((subcmd == NULL) || (subcmd_len == 0))
  {
    (void)snprintf(pcWriteBuffer, xWriteBufferLen,
        "camera diagnostics. subcommands: id, init, snap, cont, stop, status, hex, pixels\r\n");
    return pdFALSE;
  }

  if ((subcmd_len == 2) && (strncmp(subcmd, "id", 2) == 0))
  {
    uint16_t id = AppCamera_ReadID();
    const char *name = "Unknown";
    if (id == 0x2640U || id == 0x2642U) name = "OV2640";
    (void)snprintf(pcWriteBuffer, xWriteBufferLen,
        "Camera ID: 0x%04X (%s)\r\n", id, name);
    return pdFALSE;
  }

  if ((subcmd_len == 4) && (strncmp(subcmd, "init", 4) == 0))
  {
    AppCamera_Init();
    if (AppCamera_IsInitialized())
    {
      (void)snprintf(pcWriteBuffer, xWriteBufferLen,
          "Camera initialized: OV2640, %ux%u\r\n",
          (unsigned int)AppCamera_GetWidth(),
          (unsigned int)AppCamera_GetHeight());
    }
    else
    {
      uint16_t id = AppCamera_ReadID();
      if (id == 0xFFFFU)
      {
        (void)snprintf(pcWriteBuffer, xWriteBufferLen,
            "SCCB bus error (id=0xFFFF). Check wiring.\r\n");
      }
      else
      {
        (void)snprintf(pcWriteBuffer, xWriteBufferLen,
            "Unknown sensor ID: 0x%04X\r\n", id);
      }
    }
    return pdFALSE;
  }

  if ((subcmd_len == 4) && (strncmp(subcmd, "snap", 4) == 0))
  {
    if (!AppCamera_IsInitialized())
    {
      (void)snprintf(pcWriteBuffer, xWriteBufferLen,
          "Camera not initialized. Run 'camera init' first.\r\n");
      return pdFALSE;
    }

    if (AppCamera_Snapshot() != 0)
    {
      (void)snprintf(pcWriteBuffer, xWriteBufferLen, "Snapshot start failed.\r\n");
      return pdFALSE;
    }

    (void)snprintf(pcWriteBuffer, xWriteBufferLen,
        "Snapshot triggered. Check frame state with 'camera status'.\r\n"
        "Use 'camera hex' to view data.\r\n");
    return pdFALSE;
  }

  if ((subcmd_len == 4) && (strncmp(subcmd, "cont", 4) == 0))
  {
    if (!AppCamera_IsInitialized())
    {
      (void)snprintf(pcWriteBuffer, xWriteBufferLen,
          "Camera not initialized. Run 'camera init' first.\r\n");
      return pdFALSE;
    }

    if (AppCamera_Continuous() != 0)
    {
      (void)snprintf(pcWriteBuffer, xWriteBufferLen, "Continuous start failed.\r\n");
      return pdFALSE;
    }

    (void)snprintf(pcWriteBuffer, xWriteBufferLen,
        "Continuous capture started.\r\n");
    return pdFALSE;
  }

  if ((subcmd_len == 4) && (strncmp(subcmd, "stop", 4) == 0))
  {
    AppCamera_Stop();
    (void)snprintf(pcWriteBuffer, xWriteBufferLen, "Camera stopped.\r\n");
    return pdFALSE;
  }

  if ((subcmd_len == 6) && (strncmp(subcmd, "status", 6) == 0))
  {
    AppCamera_BuildStatus(pcWriteBuffer, xWriteBufferLen);
    return pdFALSE;
  }

  if ((subcmd_len == 3) && (strncmp(subcmd, "hex", 3) == 0))
  {
    const char *off_str;
    const char *len_str;
    BaseType_t off_len = 0;
    BaseType_t arg2_len = 0;
    uint32_t offset = 0U;
    uint32_t length = 0U;

    if (!AppCamera_IsInitialized())
    {
      (void)snprintf(pcWriteBuffer, xWriteBufferLen,
          "Camera not initialized. Run 'camera init' first.\r\n");
      return pdFALSE;
    }

    off_str = FreeRTOS_CLIGetParameter(pcCommandString, 2U, &off_len);
    len_str = FreeRTOS_CLIGetParameter(pcCommandString, 3U, &arg2_len);

    if ((off_str != NULL) && (off_len > 0))
    {
      offset = (uint32_t)strtoul(off_str, NULL, 0);
    }

    if ((len_str != NULL) && (arg2_len > 0))
    {
      length = (uint32_t)strtoul(len_str, NULL, 0);
    }

    AppCamera_HexDump(pcWriteBuffer, xWriteBufferLen, offset, length);
    return pdFALSE;
  }

  if ((subcmd_len == 6) && (strncmp(subcmd, "pixels", 6) == 0))
  {
    const char *x_str, *y_str, *w_str, *h_str;
    BaseType_t arg_len = 0;
    uint16_t x = 0U, y = 0U, w = 1U, h = 1U;

    if (!AppCamera_IsInitialized())
    {
      (void)snprintf(pcWriteBuffer, xWriteBufferLen,
          "Camera not initialized. Run 'camera init' first.\r\n");
      return pdFALSE;
    }

    x_str = FreeRTOS_CLIGetParameter(pcCommandString, 2U, &arg_len);
    if ((x_str != NULL) && (arg_len > 0)) x = (uint16_t)strtoul(x_str, NULL, 0);

    y_str = FreeRTOS_CLIGetParameter(pcCommandString, 3U, &arg_len);
    if ((y_str != NULL) && (arg_len > 0)) y = (uint16_t)strtoul(y_str, NULL, 0);

    w_str = FreeRTOS_CLIGetParameter(pcCommandString, 4U, &arg_len);
    if ((w_str != NULL) && (arg_len > 0)) w = (uint16_t)strtoul(w_str, NULL, 0);

    h_str = FreeRTOS_CLIGetParameter(pcCommandString, 5U, &arg_len);
    if ((h_str != NULL) && (arg_len > 0)) h = (uint16_t)strtoul(h_str, NULL, 0);

    if (w == 0U) w = 1U;
    if (h == 0U) h = 1U;

    AppCamera_PixelSample(pcWriteBuffer, xWriteBufferLen, x, y, w, h);
    return pdFALSE;
  }

  if ((subcmd_len == 6) && (strncmp(subcmd, "verify", 6) == 0))
  {
    if (!AppCamera_IsInitialized())
    {
      (void)snprintf(pcWriteBuffer, xWriteBufferLen,
          "Camera not initialized. Run 'camera init' first.\r\n");
      return pdFALSE;
    }

    AppCamera_BuildVerify(pcWriteBuffer, xWriteBufferLen);
    return pdFALSE;
  }

  (void)snprintf(pcWriteBuffer, xWriteBufferLen,
      "Unknown camera command: %.*s\r\n", subcmd_len, subcmd);
  return pdFALSE;
}

static BaseType_t CLI_SdCommand(char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString)
{
  const char *subcmd;
  BaseType_t subcmd_len = 0;

  if ((pcWriteBuffer == NULL) || (xWriteBufferLen == 0U))
  {
    return pdFALSE;
  }

  subcmd = FreeRTOS_CLIGetParameter(pcCommandString, 1U, &subcmd_len);

  if ((subcmd == NULL) || (subcmd_len == 0))
  {
    (void)snprintf(pcWriteBuffer, xWriteBufferLen,
        "sd subcommands: mount, umount, info, dir, cat\r\n");
    return pdFALSE;
  }

  if ((subcmd_len == 5) && (strncmp(subcmd, "mount", 5) == 0))
  {
    if (AppSD_Mount())
    {
      (void)snprintf(pcWriteBuffer, xWriteBufferLen, "SD mounted OK.\r\n");
    }
    else
    {
      (void)snprintf(pcWriteBuffer, xWriteBufferLen,
          "SD mount FAILED. Check card inserted, formatted FAT32/exFAT.\r\n");
    }
    return pdFALSE;
  }

  if ((subcmd_len == 6) && (strncmp(subcmd, "umount", 6) == 0))
  {
    AppSD_Umount();
    (void)snprintf(pcWriteBuffer, xWriteBufferLen, "SD unmounted.\r\n");
    return pdFALSE;
  }

  if ((subcmd_len == 4) && (strncmp(subcmd, "info", 4) == 0))
  {
    AppSD_BuildInfo(pcWriteBuffer, xWriteBufferLen);
    return pdFALSE;
  }

  if ((subcmd_len == 3) && (strncmp(subcmd, "dir", 3) == 0))
  {
    AppSD_ListDir(pcWriteBuffer, xWriteBufferLen, "/");
    return pdFALSE;
  }

  if ((subcmd_len == 3) && (strncmp(subcmd, "cat", 3) == 0))
  {
    const char *path;
    BaseType_t path_len = 0;

    path = FreeRTOS_CLIGetParameter(pcCommandString, 2U, &path_len);
    if ((path == NULL) || (path_len == 0U))
    {
      (void)snprintf(pcWriteBuffer, xWriteBufferLen,
          "Usage: sd cat <filename>\r\n");
      return pdFALSE;
    }

    {
      char filepath[128];
      if ((size_t)path_len >= sizeof(filepath))
      {
        (void)snprintf(pcWriteBuffer, xWriteBufferLen, "Path too long.\r\n");
        return pdFALSE;
      }
      memcpy(filepath, path, (size_t)path_len);
      filepath[path_len] = '\0';
      AppSD_ReadFile(pcWriteBuffer, xWriteBufferLen, filepath, 2048U);
    }

    return pdFALSE;
  }

  (void)snprintf(pcWriteBuffer, xWriteBufferLen,
      "Unknown sd command: %.*s\r\n", subcmd_len, subcmd);
  return pdFALSE;
}
