#include "app_qspi.h"

#include <stdio.h>
#include <string.h>

#include "w25q64.h"
#include "lfs.h"
#include "lfs_port.h"

#define LFS_BLOCK_SIZE  4096U

static lfs_t g_lfs;
static bool  g_lfs_mounted = false;

bool AppQSPI_Init(void)
{
  if (!W25Q64_Init())
    return false;

  return true;
}

bool AppQSPI_IsPresent(void)
{
  return W25Q64_IsPresent();
}

bool AppQSPI_Mount(void)
{
  int err;

  if (g_lfs_mounted) return true;
  if (!W25Q64_IsPresent()) return false;

  err = lfs_mount(&g_lfs, &g_lfs_cfg);
  if (err != LFS_ERR_OK)
  {
    return false;
  }

  g_lfs_mounted = true;
  return true;
}

void AppQSPI_Umount(void)
{
  if (g_lfs_mounted)
  {
    lfs_unmount(&g_lfs);
    g_lfs_mounted = false;
  }
}

bool AppQSPI_IsMounted(void)
{
  return g_lfs_mounted;
}

bool AppQSPI_Format(void)
{
  int err;

  if (g_lfs_mounted)
  {
    lfs_unmount(&g_lfs);
    g_lfs_mounted = false;
  }

  err = lfs_format(&g_lfs, &g_lfs_cfg);
  if (err != LFS_ERR_OK) return false;

  err = lfs_mount(&g_lfs, &g_lfs_cfg);
  if (err != LFS_ERR_OK) return false;

  g_lfs_mounted = true;
  return true;
}

size_t AppQSPI_ListDir(char *buf, size_t buf_len)
{
  lfs_dir_t dir;
  struct lfs_info info;
  int err;
  size_t used = 0U;

  if ((buf == NULL) || (buf_len == 0U)) return 0U;
  if (!g_lfs_mounted)
  {
    (void)snprintf(buf, buf_len, "Flash not mounted. Use 'flash mount' first.\r\n");
    return strlen(buf);
  }

  err = lfs_dir_open(&g_lfs, &dir, "/");
  if (err != LFS_ERR_OK)
  {
    (void)snprintf(buf, buf_len, "Cannot open root dir (err=%d)\r\n", err);
    return strlen(buf);
  }

  for (;;)
  {
    err = lfs_dir_read(&g_lfs, &dir, &info);
    if (err <= 0) break;

    if ((info.name[0] == '.') && (info.name[1] == '\0' || (info.name[1] == '.' && info.name[2] == '\0')))
      continue;

    if (used < buf_len)
    {
      int written;
      if (info.type == LFS_TYPE_DIR)
        written = snprintf(buf + used, buf_len - used, "  [DIR]  %s\r\n", info.name);
      else
        written = snprintf(buf + used, buf_len - used, "  %6lu  %s\r\n", (unsigned long)info.size, info.name);
      if (written > 0) used += (size_t)written;
    }
  }

  lfs_dir_close(&g_lfs, &dir);
  return used;
}

size_t AppQSPI_ReadFile(char *buf, size_t buf_len, const char *path, uint32_t max_bytes)
{
  lfs_file_t file;
  int err;
  lfs_ssize_t br;
  size_t used;

  if ((buf == NULL) || (buf_len == 0U) || (path == NULL)) return 0U;
  if (!g_lfs_mounted)
  {
    (void)snprintf(buf, buf_len, "Flash not mounted. Use 'flash mount' first.\r\n");
    return strlen(buf);
  }

  err = lfs_file_open(&g_lfs, &file, path, LFS_O_RDONLY);
  if (err != LFS_ERR_OK)
  {
    (void)snprintf(buf, buf_len, "Cannot open: %s (err=%d)\r\n", path, err);
    return strlen(buf);
  }

  if (max_bytes == 0U || max_bytes > (buf_len - 1U))
    max_bytes = (uint32_t)(buf_len - 1U);

  br = lfs_file_read(&g_lfs, &file, buf, (lfs_size_t)max_bytes);
  lfs_file_close(&g_lfs, &file);

  used = (br > 0) ? (size_t)br : 0U;
  if (used < buf_len) buf[used] = '\0';
  return used;
}

const char* AppQSPI_GetName(void)
{
  return W25Q64_GetName();
}

uint32_t AppQSPI_ReadID(void)
{
  return W25Q64_ReadID();
}

size_t AppQSPI_BuildInfo(char *buf, size_t buf_len)
{
  if ((buf == NULL) || (buf_len == 0U)) return 0U;

  if (!W25Q64_IsPresent())
  {
    (void)snprintf(buf, buf_len, "Flash: NOT DETECTED\r\n");
    return strlen(buf);
  }

  if (!g_lfs_mounted)
  {
    (void)snprintf(buf, buf_len,
        "Flash: %s\r\n"
        "Capacity: %lu MB\r\n"
        "Status:  not mounted (use 'flash mount')\r\n",
        W25Q64_GetName(),
        (unsigned long)(W25Q64_GetCapacity() / 1024UL / 1024UL));
  }
  else
  {
    lfs_ssize_t used_blocks = lfs_fs_size(&g_lfs);
    uint32_t total_kb = W25Q64_GetCapacity() / 1024UL;
    uint32_t used_kb  = (used_blocks > 0) ? (uint32_t)((uint64_t)used_blocks * LFS_BLOCK_SIZE / 1024UL) : 0U;

    (void)snprintf(buf, buf_len,
        "Flash: %s\r\n"
        "Capacity: %lu MB (%lu KB)\r\n"
        "FS blocks: %lu KB used / %lu KB total\r\n"
        "Status:   mounted (LittleFS)\r\n",
        W25Q64_GetName(),
        (unsigned long)(total_kb / 1024UL), (unsigned long)total_kb,
        (unsigned long)used_kb, (unsigned long)(W25Q64_CAPACITY / 2048UL));
  }

  return strlen(buf);
}
