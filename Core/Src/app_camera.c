#include "app_camera.h"

#include <stdio.h>
#include <string.h>

#include "dcmi.h"
#include "dcmi_ov2640_cfg.h"
#include "sccb.h"

#define CAMERA_BUFFER_BYTES     (OV2640_BufferSize * 4U)
#define CAMERA_PREVIEW_LINES    16U
#define CAMERA_HEX_BYTES_LINE   16U
#define CAMERA_PREVIEW_BYTES    (CAMERA_PREVIEW_LINES * CAMERA_HEX_BYTES_LINE)

__attribute__((section(".camera_buffer"), aligned(32)))
volatile uint8_t CameraBuffer[CAMERA_BUFFER_BYTES];

static bool g_camera_initialized = false;

void AppCamera_Init(void)
{
  uint16_t id;

  if (g_camera_initialized)
  {
    return;
  }

  SCCB_GPIO_Config();
  OV2640_Reset();
  id = OV2640_ReadID();

  if ((id == 0x2640U) || (id == 0x2642U))
  {
    OV2640_Set_Pixformat(Pixformat_RGB565);
    OV2640_Config(OV2640_SVGA_Config);
    OV2640_Set_Framesize(OV2640_Width, OV2640_Height);
    OV2640_DCMI_Crop(Display_Width, Display_Height, OV2640_Width, OV2640_Height);
    g_camera_initialized = true;
  }
}

bool AppCamera_IsInitialized(void)
{
  return g_camera_initialized;
}

uint16_t AppCamera_ReadID(void)
{
  return OV2640_ReadID();
}

int8_t AppCamera_Snapshot(void)
{
  if (!g_camera_initialized)
  {
    return -1;
  }

  OV2640_DCMI_Stop();
  OV2640_DMA_Transmit_Snapshot((uint32_t)CameraBuffer, OV2640_BufferSize);
  return 0;
}

int8_t AppCamera_Continuous(void)
{
  if (!g_camera_initialized)
  {
    return -1;
  }

  OV2640_DCMI_Stop();
  OV2640_DMA_Transmit_Continuous((uint32_t)CameraBuffer, OV2640_BufferSize);
  return 0;
}

void AppCamera_Stop(void)
{
  OV2640_DCMI_Stop();
}

uint8_t AppCamera_GetFrameState(void)
{
  return DCMI_FrameState;
}

uint8_t AppCamera_GetFPS(void)
{
  return OV2640_FPS;
}

void AppCamera_ClearFrameState(void)
{
  DCMI_FrameState = 0;
}

size_t AppCamera_GetBufferSize(void)
{
  return (size_t)OV2640_BufferSize;
}

size_t AppCamera_GetWidth(void)
{
  return (size_t)Display_Width;
}

size_t AppCamera_GetHeight(void)
{
  return (size_t)Display_Height;
}

void AppCamera_HexDump(char *buf, size_t buf_len, uint32_t offset, uint32_t len)
{
  const uint8_t *cam_buf;
  uint32_t max_offset;
  uint32_t i;
  size_t used;

  if ((buf == NULL) || (buf_len == 0U))
  {
    return;
  }

  memset(buf, 0, buf_len);
  max_offset = OV2640_BufferSize * 4U;

  if (offset >= max_offset)
  {
    (void)snprintf(buf, buf_len, "error: offset 0x%lX >= max 0x%lX\r\n",
                   (unsigned long)offset, (unsigned long)max_offset);
    return;
  }

  if (len == 0U)
  {
    len = CAMERA_PREVIEW_BYTES;
  }

  if ((offset + len) > max_offset)
  {
    len = max_offset - offset;
  }

  cam_buf = (const uint8_t *)CameraBuffer;
  used = 0U;
  for (i = 0U; (i < len) && (used < buf_len); i += CAMERA_HEX_BYTES_LINE)
  {
    uint32_t j;
    int written;

    written = snprintf(buf + used, buf_len - used, "%08lX: ", (unsigned long)(offset + i));
    if (written < 0) break;
    used += (size_t)written;

    for (j = 0U; (j < CAMERA_HEX_BYTES_LINE) && ((i + j) < len) && (used < buf_len); j++)
    {
      written = snprintf(buf + used, buf_len - used, "%02X ", cam_buf[offset + i + j]);
      if (written < 0) break;
      used += (size_t)written;
    }

    if (used < buf_len)
    {
      buf[used++] = '\r';
    }
    if (used < buf_len)
    {
      buf[used++] = '\n';
    }
  }
}

void AppCamera_PixelSample(char *buf, size_t buf_len, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
  const uint8_t *cam_buf;
  uint16_t r, c;
  size_t used;
  int written;
  uint32_t max_offset;

  if ((buf == NULL) || (buf_len == 0U) || (x >= Display_Width) || (y >= Display_Height))
  {
    if ((buf != NULL) && (buf_len > 0U))
    {
      (void)snprintf(buf, buf_len, "error: out of bounds (x=%u y=%u, max=%u x %u)\r\n",
                     x, y, Display_Width, Display_Height);
    }
    return;
  }

  if (w == 0U) w = 1U;
  if (h == 0U) h = 1U;

  if ((x + w) > Display_Width)  w = Display_Width - x;
  if ((y + h) > Display_Height) h = Display_Height - y;

  cam_buf   = (const uint8_t *)CameraBuffer;
  max_offset = OV2640_BufferSize * 4U;

  used = 0U;

  for (r = 0U; (r < h) && (used < buf_len); r++)
  {
    for (c = 0U; (c < w) && (used < buf_len); c++)
    {
      uint32_t pixel_offset;
      uint16_t pixel;

      pixel_offset = ((uint32_t)(y + r) * Display_Width + (uint32_t)(x + c)) * 2U;

      if (pixel_offset + 1U >= max_offset) break;

      pixel = (uint16_t)(((uint16_t)cam_buf[pixel_offset + 1U] << 8) | cam_buf[pixel_offset]);

      written = snprintf(buf + used, buf_len - used,
                         "(%3u,%3u) 0x%04X  ",
                         x + c, y + r, pixel);
      if (written < 0) break;
      used += (size_t)written;
    }
    if (used < buf_len)
    {
      buf[used++] = '\r';
    }
    if (used < buf_len)
    {
      buf[used++] = '\n';
    }
  }
}

void AppCamera_BuildStatus(char *buf, size_t buf_len)
{
  if ((buf == NULL) || (buf_len == 0U))
  {
    return;
  }

  if (!g_camera_initialized)
  {
    (void)snprintf(buf, buf_len, "Camera: NOT INITIALIZED\r\n");
    return;
  }

  (void)snprintf(buf, buf_len,
      "Camera: OV2640\r\n"
      "Buffer: 0x%08lX (%lu words, %lu bytes)\r\n"
      "Frame:  %ux%u\r\n"
      "FPS:    %u\r\n"
      "State:  %u (1=frame ready)\r\n",
      (unsigned long)&CameraBuffer,
      (unsigned long)OV2640_BufferSize,
      (unsigned long)(OV2640_BufferSize * 4U),
      Display_Width, Display_Height,
      OV2640_FPS,
      DCMI_FrameState);
}

void AppCamera_BuildVerify(char *buf, size_t buf_len)
{
  const volatile uint8_t *cam;
  uint32_t i, total;
  uint16_t min_val, max_val;
  uint32_t sum, zero_cnt, sat_cnt;
  uint16_t samples[5][5];

  if ((buf == NULL) || (buf_len == 0U))
  {
    return;
  }

  if (!g_camera_initialized)
  {
    (void)snprintf(buf, buf_len, "Camera not initialized\r\n");
    return;
  }

  cam    = (const volatile uint8_t *)CameraBuffer;
  total  = (uint32_t)OV2640_BufferSize * 2U; /* pixel count = words * 2 */
  min_val = 0xFFFFU;
  max_val = 0x0000U;
  sum     = 0U;
  zero_cnt = 0U;
  sat_cnt  = 0U;

  for (i = 0U; i < total; i++)
  {
    uint16_t p = (uint16_t)(((uint16_t)cam[i * 2U + 1U] << 8) | cam[i * 2U]);

    if (p < min_val) min_val = p;
    if (p > max_val) max_val = p;
    sum += (uint32_t)p;

    if (p == 0x0000U) zero_cnt++;
    if (p == 0xFFFFU) sat_cnt++;
  }

  {
    uint16_t cx = (uint16_t)(Display_Width / 2U);
    uint16_t cy = (uint16_t)(Display_Height / 2U);
    uint16_t r, c;
    for (r = 0U; r < 5U; r++)
    {
      for (c = 0U; c < 5U; c++)
      {
        uint32_t off = ((uint32_t)(cy - 2U + r) * Display_Width + (uint32_t)(cx - 2U + c)) * 2U;
        samples[r][c] = (uint16_t)(((uint16_t)cam[off + 1U] << 8) | cam[off]);
      }
    }
  }

  (void)snprintf(buf, buf_len,
      "Image Verify (%ux%u, %lu pixels):\r\n"
      "  Min:    0x%04X\r\n"
      "  Max:    0x%04X\r\n"
      "  Avg:    %lu\r\n"
      "  Zero:   %lu (%.1f%%)\r\n"
      "  Sat:    %lu (%.1f%%)\r\n"
      "  Center 5x5 samples:\r\n"
      "    %04X %04X %04X %04X %04X\r\n"
      "    %04X %04X %04X %04X %04X\r\n"
      "    %04X %04X %04X %04X %04X\r\n"
      "    %04X %04X %04X %04X %04X\r\n"
      "    %04X %04X %04X %04X %04X\r\n",
      Display_Width, Display_Height, (unsigned long)total,
      min_val, max_val,
      (unsigned long)(total > 0U ? sum / total : 0U),
      (unsigned long)zero_cnt, total > 0U ? (float)zero_cnt * 100.0f / (float)total : 0.0f,
      (unsigned long)sat_cnt,  total > 0U ? (float)sat_cnt  * 100.0f / (float)total : 0.0f,
      samples[0][0], samples[0][1], samples[0][2], samples[0][3], samples[0][4],
      samples[1][0], samples[1][1], samples[1][2], samples[1][3], samples[1][4],
      samples[2][0], samples[2][1], samples[2][2], samples[2][3], samples[2][4],
      samples[3][0], samples[3][1], samples[3][2], samples[3][3], samples[3][4],
      samples[4][0], samples[4][1], samples[4][2], samples[4][3], samples[4][4]);
}

