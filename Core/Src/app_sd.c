#include "app_sd.h"

#include <stdio.h>
#include <string.h>

#include "ff.h"
#include "bsp_sd.h"

static FATFS g_sd_fs;
static bool  g_sd_mounted = false;

bool AppSD_Mount(void)
{
  FRESULT res;

  if (g_sd_mounted)
  {
    return true;
  }

  if (BSP_SD_Init(0) != BSP_ERROR_NONE)
  {
    return false;
  }

  memset(&g_sd_fs, 0, sizeof(g_sd_fs));
  res = f_mount(&g_sd_fs, "0:", 1);
  if (res != FR_OK)
  {
    return false;
  }

  g_sd_mounted = true;
  return true;
}

void AppSD_Umount(void)
{
  if (g_sd_mounted)
  {
    f_mount(NULL, "0:", 1);
    g_sd_mounted = false;
  }
}

bool AppSD_IsMounted(void)
{
  return g_sd_mounted;
}

size_t AppSD_ListDir(char *buf, size_t buf_len, const char *path)
{
  FRESULT res;
  DIR dir;
  FILINFO fno;
  size_t used = 0U;

  if ((buf == NULL) || (buf_len == 0U))
  {
    return 0U;
  }

  if (!g_sd_mounted)
  {
    (void)snprintf(buf, buf_len, "SD not mounted. Use 'sd mount' first.\r\n");
    return strlen(buf);
  }

  res = f_opendir(&dir, (path != NULL) ? path : "/");
  if (res != FR_OK)
  {
    (void)snprintf(buf, buf_len, "Cannot open dir: %s (err=%d)\r\n",
                   (path != NULL) ? path : "/", (int)res);
    return strlen(buf);
  }

  for (;;)
  {
    res = f_readdir(&dir, &fno);
    if ((res != FR_OK) || (fno.fname[0] == 0U))
    {
      break;
    }

    if (used < buf_len)
    {
      int written;

      if (fno.fattrib & AM_DIR)
      {
        written = snprintf(buf + used, buf_len - used,
                           "  [DIR]  %s\r\n", fno.fname);
      }
      else
      {
        written = snprintf(buf + used, buf_len - used,
                           "  %6lu  %s\r\n", (unsigned long)fno.fsize, fno.fname);
      }

      if (written > 0)
      {
        used += (size_t)written;
      }
    }
  }

  f_closedir(&dir);

  if (used < buf_len)
  {
    buf[used++] = '\r';
    buf[used]   = '\n';
  }

  return used;
}

size_t AppSD_ReadFile(char *buf, size_t buf_len, const char *path, uint32_t max_bytes)
{
  FRESULT res;
  FIL fil;
  UINT br;
  size_t used = 0U;

  if ((buf == NULL) || (buf_len == 0U) || (path == NULL))
  {
    return 0U;
  }

  if (!g_sd_mounted)
  {
    (void)snprintf(buf, buf_len, "SD not mounted. Use 'sd mount' first.\r\n");
    return strlen(buf);
  }

  res = f_open(&fil, path, FA_READ);
  if (res != FR_OK)
  {
    (void)snprintf(buf, buf_len, "Cannot open: %s (err=%d)\r\n", path, (int)res);
    return strlen(buf);
  }

  if (max_bytes == 0U || max_bytes > (buf_len - 1U))
  {
    max_bytes = (uint32_t)(buf_len - 1U);
  }

  res = f_read(&fil, buf, (UINT)max_bytes, &br);
  f_close(&fil);

  if (res != FR_OK)
  {
    (void)snprintf(buf, buf_len, "Read error: %d\r\n", (int)res);
    return strlen(buf);
  }

  used = (size_t)br;
  if (used < buf_len)
  {
    buf[used] = '\0';
  }

  return used;
}

size_t AppSD_BuildInfo(char *buf, size_t buf_len)
{
  HAL_SD_CardInfoTypeDef info;
  DWORD free_clusters;
  FATFS *fs_ptr;

  if ((buf == NULL) || (buf_len == 0U))
  {
    return 0U;
  }

  if (!g_sd_mounted)
  {
    (void)snprintf(buf, buf_len, "SD not mounted. Use 'sd mount' first.\r\n");
    return strlen(buf);
  }

  memset(&info, 0, sizeof(info));
  BSP_SD_GetCardInfo(0, &info);

  free_clusters = 0U;
  fs_ptr = &g_sd_fs;
  f_getfree("0:", &free_clusters, &fs_ptr);

  (void)snprintf(buf, buf_len,
      "SD Card Info:\r\n"
      "  CardType:     %lu\r\n"
      "  BlockSize:    %lu bytes\r\n"
      "  BlockNbr:     %lu\r\n"
      "  Capacity:     %lu MB\r\n"
      "  FreeClusters: %lu\r\n"
      "  FreeSpace:    ~%lu KB\r\n",
      (unsigned long)info.CardType,
      (unsigned long)info.BlockSize,
      (unsigned long)info.BlockNbr,
      (unsigned long)(((uint64_t)info.BlockNbr * info.BlockSize) / (1024UL * 1024UL)),
      (unsigned long)free_clusters,
      (unsigned long)((uint64_t)free_clusters * g_sd_fs.csize * 512UL / 1024UL));

  return strlen(buf);
}
